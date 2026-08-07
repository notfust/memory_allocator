# Технический аудит и рекомендации по развитию memory_allocator

**Версия проекта:** 0.3.0  
**Первичный аудит:** 2026-08-06  
**Переоценка по `AGENTS.md`:** 2026-08-07  
**Целевой контекст:** Infineon AURIX TC397XA (TriCore, HighTec `tricore-gcc`); host-сборка Windows 10 предназначена для разработки и тестирования. Язык — строго C99; сборка — GNU Make.

## Пояснение изменений после переоценки

Большинство замечаний о корректности остаются актуальными: требования проекта их не отменяют. Изменены архитектурные рекомендации, которые прежде не учитывали заданные ограничения.

- Рекомендация `_Alignas(max_align_t)` **заменена**: это C11, а проект ограничен C99. Выравнивание должно задаваться через тип C99-совместимого buffer-wrapper либо platform layer; конкретное значение следует определить и подтвердить для TriCore ABI.
- Рекомендация о CMake **удалена**: проект требует GNU Make. Нужен Makefile для TriCore и host, debug и release.
- «Встроенный heap 2048 байт» как целевая модель **заменён** на передачу явного внешнего региона при инициализации. Это прямо соответствует требованиям проекта и связывает linker/startup только с platform layer.
- Упоминание штатного allocator RTOS **понижено до необязательного сравнения**, а не пути интеграции: проект должен работать без ОС и libc.
- TLSF, pools и arena сохранены как варианты выбора после профилирования; переход на них не должен быть автоматическим. Для радара критичны подтверждённые границы задержки, RAM и паттерны allocations.
- Добавлены требования к C99-переносимости, TriCore/HighTec-валидации, platform abstraction и GNU Make.

## Итоговая оценка

**Общая оценка: 2/10**  
**Уровень:** учебный прототип.

Реализация показывает базовую схему first-fit allocator со split/coalescing, но пока не соответствует заявленной цели детерминированного standalone-компонента для AURIX. До интеграции в алгоритм radar tracking требуются исправления корректности, измерения и тестовое обоснование.

## Блокирующие дефекты

### 1. Несоответствие linkage `memory_alloc()` — устранено в рабочем дереве, но должно быть зафиксировано

- **Файл:** `sources/memory_allocator.c:145`.
- **Первоначальная проблема:** определение было `static`, а объявление в `includes/memory_allocator.h:63` — external.
- **Текущее состояние:** в рабочем дереве определение уже имеет external linkage; исходная ошибка clang устранена.
- **Действие:** сохранить это изменение отдельным проверяемым commit и добавить host-сборку в Makefile, чтобы регрессия сразу выявлялась.

### 2. Выравнивание heap и возвращаемых указателей не гарантировано

- **Файлы:** `sources/memory_allocator.c:44, 60-63, 137, 152`.
- **Проблема:** `uint8_t heap[]` гарантированно выровнен лишь на 1 байт; запрос округляется до 4 байт без документированной связи с TriCore ABI и типами клиента.
- **Последствия:** undefined behavior при доступе к `memory_block_t`; на target возможны trap/некорректный доступ и порча данных для типов с более строгим alignment.
- **Исправление с учётом C99:** core не должен объявлять внутренний `uint8_t heap[]`. Platform layer передаёт region с документированным alignment. В C99 для локального статического region использовать union с членом, задающим требуемое выравнивание, либо platform-specific declaration, изолированное под `TRICORE_TARGET`. Размер выравнивания должен быть константой API, обоснованной и проверенной на `tricore-gcc`; каждый allocation округлять с контролем переполнения.

### 3. `memory_free(NULL)` содержит undefined behavior

- **Файл:** `sources/memory_allocator.c:187-190`.
- **Проблема:** заголовок вычисляется арифметикой над `NULL` до проверки.
- **Последствие:** undefined behavior.
- **Исправление:** первым действием выполнить `if (ptr == NULL) { return MEMORY_STATUS_OK; }` либо безопасный `return`, если API сознательно сохранит `void`. Для нового API предпочтительнее явный статус.

### 4. Double free не детектируется и способен повредить heap

- **Файл:** `sources/memory_allocator.c:192-211`.
- **Проблема:** `is_free` не проверяется перед освобождением; после coalescing старый header может уже не быть элементом списка.
- **Последствия:** повреждение размеров/ссылок, пересекающиеся allocations, запись по ошибочному адресу.
- **Исправление:** проверять, что pointer соответствует payload текущего занятого блока в списке; при ошибке не изменять heap и вернуть явный `MEMORY_STATUS_INVALID_POINTER`/`MEMORY_STATUS_DOUBLE_FREE`. В production не полагаться только на magic.

### 5. Нет достаточной защиты от forged и interior pointers

- **Файл:** `sources/memory_allocator.c:115-130`.
- **Проблема:** проверяются только примерные границы, magic и размер; не проверяется принадлежность header текущему списку, непрерывность блоков и обратные ссылки.
- **Последствия:** overflow пользовательского payload может подделать metadata; `memory_free()` затем использует повреждённые `prev`/`next`.
- **Исправление:** валидация должна обходить список от первого блока с ограничением числа итераций, проверять границы и физическое расположение каждого блока, `prev`/`next`, размер и искомый payload address. Debug-проверка инвариантов обязательна; её влияние на timing должно быть документировано и отключаемо в release.

### 6. Переполнение `size_t` при выравнивании и расчётах

- **Файлы:** `sources/memory_allocator.c:60-73, 152, 158`.
- **Проблема:** сложения для выравнивания, header и split не проверяются на переполнение.
- **Последствия:** большой запрос может стать малым и привести к выходу за границы.
- **Исправление:** применять C99-совместимые предикаты перед каждым сложением: `size > SIZE_MAX - addend`. До поиска немедленно отклонять выровненный размер, который не помещается в region за вычетом header и минимального размера блока.

### 7. Проверка границ непереносима и неполна

- **Файл:** `sources/memory_allocator.c:121`.
- **Проблема:** сложение произвольного указателя может переполниться; сравнение указателей вне одного массива не является переносимой C99-проверкой.
- **Исправление:** после принятия region API проверять адреса как значения `uintptr_t` только при документированном для TriCore/host предположении о преобразовании pointer-to-integer, с проверкой переполнения; или строить валидацию только из известных адресов блоков при обходе region. Это решение нужно вынести в platform abstraction и протестировать на обоих toolchain.

## Архитектурные рекомендации

### Сильные стороны текущего дизайна

- отсутствует зависимость от malloc/free libc;
- простой алгоритм и малый объём кода облегчают review;
- split и локальное объединение свободных блоков уже присутствуют;
- host-совместимость достижима без изменения core-алгоритма.

### Основные ограничения

- singleton и жёстко встроенный region 2048 байт противоречат требованию явных внешних regions;
- core смешан с хранением platform memory;
- отсутствует контракт по ownership, повторной инициализации, alignment, ISR и concurrency;
- API скрывает ошибки `free` и не предоставляет статистику;
- `memory_merge_free_blocks()` не используется и дублирует идею coalescing;
- комментарий к `memory_block_t::size` неверно говорит о включении header;
- комментарий `memory_free()` обещает error code при `void` return.

### Целевой API

Рекомендуемая C99-модель — context, явно передаваемый region и status codes. Конкретные имена могут отличаться, но ownership и ошибки должны быть выражены явно:

```c
typedef struct memory_allocator memory_allocator_t;

memory_status_t memory_allocator_init(
    memory_allocator_t *allocator,
    void *region,
    size_t region_size);

void *memory_allocator_alloc(
    memory_allocator_t *allocator,
    size_t size);

memory_status_t memory_allocator_free(
    memory_allocator_t *allocator,
    void *ptr);
```

Alignment region является частью контракта `init`; поддерживаемое payload alignment фиксируется compile-time константой и документацией. Если требуется allocation с разным alignment, добавить C99-API с параметром `alignment` только после анализа необходимости: он усложняет split, фрагментацию и worst-case.

Platform layer выбирает linker section, startup initialization и target-specific alignment. Core не должен знать об MCU registers, interrupt controller или конкретной карте памяти.

## Производительность и real-time

First-fit требует линейного поиска:

- allocation: **O(n)** в худшем случае;
- free: локальная coalescing номинально **O(1)**, но строгая проверка pointer/list может стать **O(n)**;
- полный проход `memory_merge_free_blocks()` — **O(n)** и сейчас не применяется.

Это не даёт заранее ограниченной задержки. Для radar tracking решение о пригодности должно основываться на измерениях worst-case allocation/free при реальных паттернах и максимальном числе блоков, на host и затем на TC397XA. Не следует заявлять hard real-time пригодность без такого бюджета времени.

First-fit также имеет внешнюю фрагментацию. Header overhead и его выравнивание нужно измерить на 32-bit TriCore, а не переносить цифры host ABI.

Выбор алгоритма должен следовать профилю workload:

- fixed-size объекты — pool/slab с O(1);
- данные с фазовой lifetime — arena/region, если допустимо освобождение region целиком;
- смешанные размеры при жёстком лимите latency — оценить TLSF либо segregated free lists, сравнив code size, RAM metadata и WCET с текущей реализацией.

RTOS allocator не является базовой рекомендацией: проект должен быть standalone и без ОС. Он может служить лишь внешней точкой сравнения при интеграции в будущую RTOS-систему.

## Security и safety

Риск heap corruption сохраняется: пользовательский overflow способен повредить metadata следующего блока, а `memory_free()` может совершить некорректные записи через `prev`/`next`. Возможны use-after-free, double free, forged pointer и overlapping allocations.

Нельзя заявлять safety-critical пригодность без отдельного жизненного цикла разработки и верификационных артефактов. Для движения к практикам MISRA C:2012, CERT C, ISO 26262 и IEC 61508 потребуются:

- спецификация API, ownership, preconditions/postconditions и failure modes;
- трассируемость «требование → код → тест»;
- статический анализ с задокументированными отклонениями MISRA/CERT;
- FMEA и анализ WCET на целевом toolchain;
- независимый review критических функций;
- управляемые конфигурации HighTec toolchain и GNU Make;
- отчёты покрытий, fault-injection и регрессионных тестов.

Защитные механизмы следует разделить по сборкам, чтобы не нарушать production timing/RAM budget:

1. debug: canary/red-zone, poison-on-free, полная проверка инвариантов и диагностический callback;
2. release: минимальная постоянная проверка state/bounds и явные status codes;
3. target-specific: MPU/защита region, если memory map и safety concept позволяют.

Cookie/checksum metadata допустим как дополнительная диагностика, но не как защита от атак: в bare-metal threat model он должен быть обоснован отдельно.

## Качество инженерного исполнения

Положительное:

- компактная и читаемая основа;
- Doxygen-комментарии и clang-format;
- нет зависимости от libc allocator.

Необходимо добавить:

- README с моделью ownership, ограничениями и примером интеграции;
- GNU Makefile: host/target, debug/release и строгие предупреждения, совместимые с каждым compiler;
- core/platform/test layout;
- host unit/stress tests, allocation patterns и negative tests;
- target smoke/performance test для `tricore-gcc`/AURIX;
- CI, если выбранная инфраструктура доступна проекту;
- документированный memory layout, alignment и linker-integration;
- allocator statistics без hidden global error state.

`TRUE`/`FALSE` следует удалить: стандартный C99 `bool` уже подключён, а лишние macros повышают риск конфликтов. Все interface declarations привести к единому стилю C99 (`void *`, `void memory_free(void *ptr)`).

Host sanitizers остаются полезны для отладки при наличии совместимого компилятора, но не заменяют тестирование под HighTec/TriCore и не должны становиться зависимостью embedded-сборки.

## План улучшений по приоритету

1. Зафиксировать исправление linkage, добавить GNU Makefile и host strict build; отдельно создать target build под `tricore-gcc`.
2. Определить API ownership/initialization/error contract и перейти к context + явно передаваемому region.
3. Разделить core allocator, platform memory initialization и host test layer.
4. Задать и подтвердить C99-совместимое alignment требование для TriCore и host; добавить tests alignment.
5. Устранить UB и переполнения; добавить безопасный `NULL` path.
6. Добавить отказобезопасную валидацию invalid/interior pointer и double free с явным status.
7. Удалить либо объединить неиспользуемый `memory_merge_free_blocks()`, исправить комментарии и убрать `TRUE`/`FALSE`.
8. Создать host tests: split/coalesce, exhaustion, fragmentation, repeated patterns, boundary sizes, invalid/double free и integer overflow; добавить sanitizer-прогон как необязательную host debug цель.
9. На TC397XA измерить RAM/flash, maximum block count, allocation/free latency и фрагментацию для реального radar workload; зафиксировать acceptance criteria.
10. Только если измерения не укладываются в критерии, выбрать pool/arena/TLSF или segregated lists с документированными trade-offs и повторной WCET/RAM оценкой.

## Заключение

Проект стоит развивать как standalone C99 allocator для заданного AURIX/host контура, но не следует заменять first-fit на более сложный алгоритм без реального workload и измерений. Ближайшая цель — сделать базовый allocator корректным, region-based, проверяемым GNU Make-сборкой и измеренным на TC397XA. После этого можно обоснованно решать, достаточна ли его предсказуемость для radar tracking или необходим allocator другого класса.
