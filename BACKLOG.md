# Backlog for `memory_allocator`

Источник: [`TECHNICAL_AUDIT_RECOMMENDATIONS.md`](./TECHNICAL_AUDIT_RECOMMENDATIONS.md)

## Critical

- [ ] Исправить выравнивание region и возвращаемых указателей под C99/TriCore ABI.
  - Эффект: убирает undefined behavior на доступах и делает allocator пригодным для target.
  - Реализация: C99 union задаёт выравнивание внутреннего region; размер header и
    каждый payload округляются до `MEMORY_ALLOCATOR_ALIGNMENT`. Добавлен
    host-тест `tests/test_memory_allocator_alignment.c`. TriCore EABI задаёт
    четырёхбайтное выравнивание стандартных скалярных типов; фактическое значение
    по-прежнему выводится используемым toolchain и должно быть проверено в target
    CI с `tricore-gcc`.

- [ ] Устранить undefined behavior и переполнения `size_t` в `alloc`/`free`.
  - Эффект: предотвращает выход за границы heap при больших или некорректных запросах.
  - Зависимости: безопасная арифметика размеров, проверка `NULL` до вычислений.

- [ ] Защитить `free` от double free, forged pointer и interior pointer.
  - Эффект: исключает порчу metadata и overlapping allocations.
  - Зависимости: явная валидация блока, статусные коды ошибок, инварианты списка.

## High

- [ ] Перейти на API с явным context и внешним region.
  - Эффект: устраняет singleton-модель и делает ownership явным.
  - Зависимости: пересмотр header, init/deinit contract, документация.

- [ ] Разделить core, platform layer и test layer.
  - Эффект: упрощает переносимость между host и AURIX.
  - Зависимости: реорганизация файлов, выделение target-specific кода.

- [ ] Добавить GNU Makefile для host и target, debug и release.
  - Эффект: обеспечивает воспроизводимую сборку и раннее выявление регрессий.
  - Зависимости: выбор компиляторов, флаги предупреждений, отдельные цели.

- [ ] Создать host-тесты для корректности, boundary cases и fragmentation.
  - Эффект: покрывает split/coalesce, exhaustion, repeated patterns и invalid inputs.
  - Зависимости: новый test harness, негативные сценарии, прогон на host.

- [ ] Зафиксировать real-time contract и измерить worst-case latency.
  - Эффект: показывает, подходит ли first-fit для radar workload.
  - Зависимости: профилирование на host и TC397XA, критерии acceptance.

## Medium

- [ ] Удалить или объединить неиспользуемый `memory_merge_free_blocks()`.
  - Эффект: снижает дублирование логики и путаницу в коде.
  - Зависимости: проверка, что coalescing покрыт основной веткой `free`.

- [ ] Упростить и привести комментарии и типовые декларации к единому C99-стилю.
  - Эффект: уменьшает риск несоответствия кода и документации.
  - Зависимости: корректировка комментариев `size`, `free`, `TRUE/FALSE`.

- [ ] Добавить allocator statistics и диагностический callback.
  - Эффект: помогает измерять фрагментацию, пик использования и ошибки.
  - Зависимости: решение по API и влиянию на release timing.

- [ ] Добавить debug-защиты: canary, red-zone, poison-on-free, invariant checks.
  - Эффект: ускоряет поиск corruption и invalid access в тестах.
  - Зависимости: разделение debug/release конфигураций.

- [ ] Подтвердить и задокументировать alignment requirement отдельно для host и target.
  - Эффект: снимает неопределенность по ABI и типам клиента.
  - Зависимости: измерение на `tricore-gcc`, описание в docs и tests.

## Low

- [ ] Добавить README с моделью ownership, ограничениями и примером интеграции.
  - Эффект: облегчает использование allocator другими модулями.

- [ ] Добавить CI или локальные проверочные сценарии для host build.
  - Эффект: снижает риск регрессий между ручными проверками.

- [ ] Описать memory layout, linker integration и target-specific assumptions.
  - Эффект: делает интеграцию в AURIX-проект прозрачной.

- [ ] Рассмотреть TLSF, pools, arena или segregated lists только после измерений.
  - Эффект: дает путь к улучшению WCET, если first-fit не пройдет критерии.
