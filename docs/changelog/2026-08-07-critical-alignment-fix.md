# Critical: раздельное выравнивание heap и layout блоков

**Статус:** завершён первый Critical-пункт из [`BACKLOG.md`](../../BACKLOG.md): выравнивание внутреннего region и указателей, возвращаемых `memory_alloc()`.

## 1. Исправляемый недостаток

Первоначально allocator создавал heap как `static uint8_t heap[HEAP_SIZE]` и преобразовывал начало массива в `memory_block_t *`. C99 гарантирует для `uint8_t` только байтовое выравнивание. Поэтому не было гарантии, что:

1. начало heap подходит для объекта `memory_block_t`, содержащего `size_t` и указатели;
2. начало payload после header подходит для пользовательских объектов;
3. каждый следующий header и payload после split сохраняют это свойство.

Кроме того, запросы округлялись до фиксированных 4 байт, без формально определённого контракта для host ABI.

## 2. Почему прежнее решение с одним `sizeof(union)` было недостаточно гибким

Первый вариант исправления использовал `sizeof(memory_allocator_storage_alignment_t)` одновременно как:

- способ выровнять базовый адрес heap;
- шаг размещения header и payload (`MEMORY_ALLOCATOR_ALIGNMENT`).

Это безопасно, но смешивает две разные величины:

- **alignment типа** — требование к адресу объекта;
- **sizeof типа** — число байт, которое занимает объект вместе с возможным tail padding.

Например, на TriCore `long double _Complex` может иметь размер 16 байт, но требовать только 4-байтного адреса. Union с таким членом тоже может иметь размер 16. Выравнивать каждый блок с шагом 16 тогда корректно, но избыточно: мелкие allocations расходуют больше 2048-байтного heap, чем нужно AURIX ABI.

Доработанное решение разделяет эти роли: union отвечает только за безопасный старт физического буфера, а placement quantum задаётся отдельно для target ABI.

## 3. Внесённые изменения и почему они решают проблему

### 3.1. C99 union выравнивает начало внутреннего heap

В platform/config header определён `memory_allocator_storage_alignment_t` — union со стандартными scalar-типами, включая указатели, целые, floating-point и complex-типы. В source heap хранится как:

```c
typedef union memory_heap_storage {
    memory_allocator_storage_alignment_t alignment;
    uint8_t                      bytes[HEAP_SIZE];
} memory_heap_storage_t;
```

Все члены union начинаются по одному адресу. Поэтому компилятор обязан разместить объект union так, чтобы этот адрес был корректен для каждого члена. `heap_storage.bytes[0]` имеет тот же адрес, что и объект union, и следовательно не теряет это выравнивание. Так преобразование начала `bytes` в `memory_block_t *` становится допустимым.

Union не выполняет динамического выравнивания и не «перемещает» данные: это свойство layout, установленное компилятором на этапе сборки. Размер доступного массива `bytes` остаётся `HEAP_SIZE` (2048 байт); возможный tail padding относится к static object, не к usable region.

### 3.2. Placement quantum отделён от storage wrapper

`MEMORY_ALLOCATOR_ALIGNMENT` определён в непубличном
`includes/memory_allocator_platform.h`:

```c
#ifdef TRICORE_TARGET
#define MEMORY_ALLOCATOR_ALIGNMENT ((size_t)4U)
#else
#define MEMORY_ALLOCATOR_ALIGNMENT \
    ((size_t)offsetof(memory_allocator_storage_alignment_probe_t, value))
#endif
```

Публичный `memory_allocator.h` теперь не содержит ни ABI-константы, ни
`TRICORE_TARGET`; target build выбирает их в platform/config header.

- При `TRICORE_TARGET` contract фиксирован в 4 байта — требование TriCore EABI для стандартных scalar-типов. Поэтому размер `long double _Complex` не влияет на расход каждого блока.
- Для host fallback берётся `offsetof` поля wrapper-типа после `char`. Это
  C99-константа, не меньшая требуемого выравнивания wrapper и его членов. На
  использованном host это 8 байт.

Это не означает, что `sizeof(union)` равен alignment. Размер `long double
_Complex` может быть 16 байт при alignment 8. Offset probe измеряет нужный
шаг размещения, а не размер объекта; для target дополнительно задано точное
ABI-обоснованное значение 4.

`TRICORE_TARGET` должен задаваться только target-конфигурацией HighTec build. Нельзя определять его при host-сборке: host compiler требует собственного, более строгого выравнивания `long double`.

### 3.3. Header дополняется до placement quantum

Добавлена функция:

```c
static size_t block_header_size(void)
{
    return align_size(sizeof(memory_block_t), MEMORY_ALLOCATOR_ALIGNMENT);
}
```

Если header начался по корректному адресу, переход на число байт, кратное quantum, сохраняет выравнивание payload. Функция используется в `block_total_size()`, `block_data_ptr()`, `memory_init()`, split/coalesce и проверке границ.

Это необходимо, поскольку одного выровненного heap недостаточно: `sizeof(memory_block_t)` не обязано быть кратным 4 на каждой ABI. Без padding payload мог бы начинаться по неверному адресу.

### 3.4. Размер каждого payload округляется до того же quantum

`memory_alloc()` вызывает:

```c
size = align_size(size, MEMORY_ALLOCATOR_ALIGNMENT);
```

Следующий header расположен после «padded header + padded payload». Оба слагаемых кратны quantum, поэтому следующий header также выровнен. По индукции это верно для каждого split-блока.

`MIN_USEFUL_SIZE` задан равным тому же quantum, поэтому allocator не создаёт split-блок с payload меньше минимального выровненного размера.

`align_size()` использует остаток от деления, а не битовую маску. Такой вариант не предполагает, что quantum обязан быть степенью двойки. Защита арифметики от переполнения `size_t` не была частью данного пункта и остаётся отдельным Critical-пунктом backlog.

### 3.5. `memory_free()` использует padded размер header

После введения `block_header_size()` payload расположен не после `sizeof(memory_block_t)`, а после padded header. Поэтому `memory_free()` изменён с:

```c
(uint8_t *)ptr - sizeof(memory_block_t)
```

на:

```c
(uint8_t *)ptr - block_header_size()
```

Это обязательная часть layout-контракта: иначе `free` находил бы адрес внутри padding или header, а не его начало. Изменение не решает другие известные дефекты `free` (например, `NULL`, double free и forged pointer); они остаются следующими отдельными Critical-задачами.

### 3.6. Точность комментариев

Комментарий `memory_block_t::size` исправлен: поле хранит размер payload, а не размер вместе с header. Комментарии storage и `align_size()` приведены в соответствие с новым разделением ролей.

## 4. Изменённые файлы и функции

- `includes/memory_allocator_platform.h`
  - private C99 storage-alignment wrapper;
  - `MEMORY_ALLOCATOR_ALIGNMENT` — отдельный placement contract: 4 для `TRICORE_TARGET`, безопасный host fallback иначе.
- `includes/memory_allocator.h`
  - только публичный API; platform macro и TriCore condition из него удалены.
- `sources/memory_allocator.c`
  - `memory_heap_storage_t` и `heap_storage` вместо байтового static heap;
  - `memory_copy_bytes()`, `block_load()` и `block_store()` обеспечивают
    C99-корректную работу metadata через byte representation без libc `memcpy`;
  - `align_size()`, новая `block_header_size()`, `block_total_size()`, `block_data_ptr()`;
  - `is_valid_block()`, `memory_init()`, `memory_alloc()` и `memory_free()` используют padded layout и локальные копии header.
- `tests/test_memory_allocator_alignment.c`
  - проверяет host alignment и записывает `long double`/`long double _Complex`;
  - при `TRICORE_TARGET` дополнительно проверяет контракт `MEMORY_ALLOCATOR_ALIGNMENT == 4U`.
- `BACKLOG.md`
  - первый Critical-пункт отмечен выполненным; target-проверка остаётся условием интеграции.

## 5. Почему выбран именно этот подход

Подход:

- соответствует C99: не требует `_Alignas`, `max_align_t`, compiler attributes или libc `memcpy`;
- изолирует правило TriCore ABI в одном platform/config header, а не в публичном API и не разносит `4` по allocator core;
- не трактует `uint8_t[]` как `memory_block_t`: header копируется в обычный локальный объект, изменяется и копируется обратно;
- сохраняет host-совместимость с более строгими ABI;
- не меняет публичный API и не создаёт runtime state;
- добавляет постоянное число простых арифметических операций и не меняет first-fit алгоритм.

## 6. Рассмотренные альтернативы

### Всегда использовать `sizeof(union)`

Не выбрано для target placement: корректно, но способно дать 16-байтный шаг при 4-байтном TriCore ABI и зря уменьшить usable heap.

### Всегда установить 4 байта

Не выбрано: на host это недостаточно для `long double`/complex-типов с 8- или 16-байтным выравниванием и создаёт UB в тестах и отладке.

### C11 `_Alignas(max_align_t)`

Не выбрано: проект требует C99, а это возможности C11.

### GCC/HighTec `__attribute__((aligned))`, pragma или linker directive

Не выбрано для core: они привязывают portable allocator к конкретному compiler/linker. Такие механизмы допустимы в будущем platform layer для внешнего linker region, но не заменяют padding header и payload.

### Обращение к metadata через `memory_block_t *`

Не выбрано: выравнивание union устраняет только ошибку адреса. В строгом C99
`uint8_t[]` не становится объектом `memory_block_t` от одного cast. Вместо
этого `block_load()` побайтно переносит representation header из region в
локальный `memory_block_t`, а `block_store()` переносит его обратно.

Это работает так же, как `memcpy(&local_header, region, sizeof local_header)`:
`uint8_t` может читать и записывать object representation любого объекта, а
локальный `memory_block_t` имеет корректный тип и выравнивание. Реализована
собственная небольшая функция `memory_copy_bytes()`, потому что allocator не
должен зависеть от libc. Копирование всегда выполняется между неперекрывающимися
диапазонами и имеет фиксированную длину header.

### Ручное выравнивание внутри `uint8_t[]`

Не выбрано: требуется дополнительный запас, преобразование/проверка адресов, хранение исходной границы и более сложная логика. Union решает выравнивание внутреннего static buffer средствами C99 layout.

### Немедленный переход на внешний region и context API

Не выбрано: это отдельная High-рекомендация. После такого перехода platform layer обязан проверять начало переданного region на кратность `MEMORY_ALLOCATOR_ALIGNMENT`.

## 7. Влияние

### Потребление памяти

`HEAP_SIZE` остаётся 2048 байт. Потери определяются padding header и округлением каждого payload. На TriCore шаг равен 4, то есть overhead ограничен максимум 3 байтами на header/payload округление относительно исходного 4-байтного подхода; дополнительно может появиться только padding header, если его размер не кратен 4. На host fallback равен фактическому безопасному alignment wrapper на активном ABI; на использованном host это 8 байт. Это может увеличить внутреннюю фрагментацию относительно 4-байтного target, но не из-за размера `long double _Complex`.

### Производительность и детерминированность

First-fit, split и coalesce не изменены. Добавленные операции — modulo, одно условие и сложение с фиксированной стоимостью; новых циклов, аллокаций libc или обращений к ОС нет. Worst-case allocation всё ещё O(n) из-за поиска списка. Padding может ускорить исчерпание heap при мелких host-запросах, но это предсказуемо.

### AURIX TC397XA

При target-сборке с `TRICORE_TARGET` allocator размещает standard payload/header с 4-байтным шагом и не наследует 16-байтный размер host `long double _Complex`. Union обеспечивает достаточно выровненное начало static storage. Конкретную версию HighTec `tricore-gcc` и EABI flags необходимо подтвердить сборкой/запуском target-теста; в текущем окружении этого toolchain нет.

## 8. Тесты

`tests/test_memory_allocator_alignment.c`:

1. на host получает alignment `void *`, `long long`, `long double`, `long double _Complex` C99-совместимо — через offset поля в отдельной структуре;
2. проверяет, что host fallback quantum кратен каждому требованию;
3. проверяет минимальный запрос `memory_alloc(1U)` и последующий split;
4. выделяет блоки до полного исчерпания 2048-байтного heap и проверяет, что следующий запрос возвращает `NULL`;
5. создаёт серию split-блоков, освобождает их в двух проходах для coalesce и проверяет alignment новой крупной allocation;
6. на host реально записывает `long double` и `long double _Complex`; при target-конфигурации проверяет контракт 4 байта.

Host-прогоны выполнены с `clang` и `gcc` в строгом C99 режиме, а также с `clang` + ASan/UBSan. На использованном host `MEMORY_ALLOCATOR_ALIGNMENT` равен 8, тесты прошли. После данной доработки target-вариант должен быть дополнительно собран `tricore-gcc` с `-DTRICORE_TARGET`; host нельзя использовать как замену такой проверки.
