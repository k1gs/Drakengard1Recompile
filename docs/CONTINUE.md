# Продолжение работы

Этот файл — постоянная точка входа для любого агента с доступом к репозиторию.
Запрос «продолжи» означает: восстановить актуальное состояние из файлов и
продолжить реализацию первого подтверждённого блокера с проверкой результата.
Не ограничивайся пересказом журнала или планом. При этом такой запрос сам по себе
не разрешает force-push, публикацию приватных данных или обход поведения игры.

## Порядок чтения

1. `AGENTS.md`.
2. Последние разделы `CURRENT_STATE.md`, если файл доступен. Это локальный
   игнорируемый журнал; в свежем клоне его может не быть.
3. Верхний актуальный раздел `docs/HANDOFF_2026-10-24_CONTINUATION.md`.
4. Последние актуальные разделы `docs/IOP_BOOTSTRAP.md` и
   `docs/RESEARCH_WORKFLOW.md`.
5. `docs/ROADMAP.md`, если он существует. Отсутствие этого файла не блокирует
   продолжение: используй журнал и актуальный handoff.

Сначала проверь `git status`, текущий HEAD и реальные процессы native/reference.
Более новые результаты журнала и явные указания пользователя важнее checkpoint
ниже. Не запускай старый номер эксперимента повторно и не перезаписывай evidence.

## Актуальный checkpoint: 2026-10-25

- Предыдущая модель закончила scheduler298154 fix в cf3692d. Повторная сверка
  lift008 подтверждает owning fn297E44, реальный fallthrough298154 и все12
  resume labels в switch/dispatch table. Native241 уже занят; не повторяй его.
- Реализован следующий подтверждённый контракт: normal DMA9 из настоящей shared
  VU0 data RAM в SPR. DMAE/suspend, source bounds, SPR wrap, регистры/STR и cause9
  после копирования; никаких отдельных буферов/нулей/read-side completion.
- Native244 реально прошёл прежний wait и logged минимум16 VU0→SPR completion;
  source/destination hashes совпадают и отличаются от нулевого payload.
  В trace есть возврат прежнего восстановленного leaf к2983E8.
- Bootstrap12/12 и real Release/PCH build PASS. Run244 exit5/no timeout.
  FinalGS640x511RGB128 gray — не gameplay. Меню/Yes подтверждены реальными
  GS-кадрами и ordinary SIO2 press/release. Host audio/saves/recovery не доказаны.
- Новый target **0x29353C, RA0x2984EC**, owner0x29322C. Его карта заканчивается
  ровно на missing target после alternate-entry jump-delay exit. Требуется
  проверить реальные инструкции/epilogue и bounded recovery; alias запрещён.
- Источник, hashes, точные ограничения и evidence pointers — в верхнем handoff.
  .vscode/Splash остаются нетронутыми; upstream/generated/input read-only.

## Следующий эксперимент

Следующий245 (проверь свободный номер): owning leaf0x29322C, реальный epilogue,
read-only bounded flow recovery существующим Ghidra workflow; свежие map/lift,
сверка checkpoint/resume dispatch, поведенческие тесты, настоящий runner и
native245. Не угадывай конец по соседнему fragment без проверки инструкций,
не добавляй aliases/пустой epilogue/успешные stubs. Старый lift008 не редактируй.

Меню244: actual GS title9000→Start, menu9600→Cross, NORMAL10200→Left,
highlighted Yes10800→Cross;700ms и SIO2 press/release. Проверяй изображения,
не нажимай вслепую по номеру кадра. Private observer work/spr-vu0-menu-244.py
сканирует доступные captures, не ждёт отсутствующий600, и принимает новый run ID.
Его ROIs/reference images checkpoint-specific; свежий вывод не перезаписывай.
Промпт — docs/NEXT_MODEL_PROMPT.md.

## Инструменты и границы

- Computer Use не используй. Управление — штатный host pad protocol,
  кадры — прямые GS-захваты. Не внедряй OS input и не меняй guest ready flags.
- Если доступны навыки `ps2-recomp-research` и `ps2-frame-investigation`, прочитай
  и используй их. Если другая среда не поддерживает эти навыки, следуй правилам
  этого репозитория; недоступность локального навыка сама по себе не причина
  прекращать работу.
- Субагенты необязательны. Если делегируешь, пользователь разрешил только
  GPT-5.6 Luna; проверь их выводы самостоятельно. Если такой модели нет,
  работай самостоятельно.
- Vulkan — будущий backend. Сейчас продолжай фактический следующий blocker из нового checkpoint;
  ADMA/FX уже прошли прежние границы. Не начинай миграцию рендера вместо
  исправления установленного контракта.
- Upstream в `tools/ps2recomp` и generated guest code не редактируй. Исправляй
  оригинальные адаптеры/генераторы/overlays. Сохраняй `.vscode/` и чужие изменения.
- Исходные уже проверенные inputs ранее были подтверждены как полные. Сохраняй
  их read-only и проверяй закреплённые hashes существующими средствами. Новые
  поступившие файлы не исследуй до подтверждения их полноты пользователем.
- Pinned PCSX2 v2.8.2 reference находится локально в
  `work/ipu-reference-001/`. Используй его для сверки контрактов, не копируй
  реализацию декодера в публичный исходник.
- Никаких success stubs, выдуманных completion/IRQ, подстановок start-code,
  исправления RA или патчей ready/input flags ради продвижения.
- Не пропускай фильм патчем гостевого кода. Автоматизация предупреждения карты
  памяти обычными кнопками уже разрешена и реализована.

## Следующий ограниченный прогон

На момент checkpoint следующий номер —245. Сначала проверь, что каталог
`work/native-run-245` свободен и другой native/reference процесс не работает.
Если номер уже занят, выбери следующий свободный, сохрани прежние результаты.

Настоящий executable: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`recomp_smoke.exe` не заменяет его. После изменения runtime собери настоящий
runner, проверь exit code сборки и запиши SHA-256 проверяемого executable.

```powershell
cmake --build build/native --config Release --target ps2EntryRunner --parallel 4
python scripts/run_native.py --input-complete --run 245 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 1024
```

Пока launcher работает, отдельным процессом запусти:

```powershell
python scripts/native_start.py --run 245 --timeout 300
```

Не жди завершения launcher перед запуском controller helper. Он проходит
предупреждение обычными Left/Cross, подтверждает SIO2 press/release и прекращает
стартовый ввод при первом IPU command. Это checkpoint-specific рецепт,
не универсальное распознавание диалога. Frame1200 в214–216 ещё показывал
Accessing; не возвращайся к старому слишком раннему вводу.
В218 helper исчерпал default180s после пяти попыток, но runner позже достиг
IPU и нового motion-code отказа; не выдавай exit2 helper за успех. Поэтому
здесь указан разрешённый предел300s; runner по-прежнему ограничен300s.

При необходимости перед запуском launcher установи
`DRAKENGARD_FRAME_INTERVAL=600`: это opt-in прямые GS-снимки с пределом32 файла.
Запиши диагностическую опцию в evidence. Не выдавай GS-снимок за независимую
проверку окна. Native и reference запускай последовательно, каждый native
эксперимент ограничивай максимум300 секундами.

## Проверка и завершение этапа

Отложенная задача пользователя: `native/Splash.png` должен показываться в host
окне сразу после запуска executable вместо начального розового фона. Asset
сохранён без изменений; интеграция ещё не выполнена. Host splash нельзя выдавать
за кадр игры. При публикации сначала проверь правила assets/public-tree.

Для исправленного контракта добавь детерминированные поведенческие регрессии,
собери соответствующие targets, затем проверь путь настоящим bounded native
прогоном. Синтетические тесты не доказывают меню или gameplay.

Перед остановкой обязательно обнови:

- `CURRENT_STATE.md`: изменения, команды/версии, hashes, evidence paths,
  exit vs timeout, первый оставшийся блокер и один следующий эксперимент.
- Верхний актуальный раздел handoff: то, что сможет прочитать агент в свежем
  клоне без приватного журнала и истории чата.
- Этот файл, если изменились точка входа, команда или актуальный checkpoint.

Не записывай новый прогресс в глобальную память ассистента без отдельной просьбы.
Публикуй только разрешённый source/docs/tests checkpoint, проверяй staged diff и
`python scripts/check_public_tree.py`. Запрос «продолжи» не отменяет явного
ограничения пользователя на commit/push; текущий пользователь разрешил push
проверенного checkpoint и дальнейшее продолжение. Публикуй focused validated
изменения обычным push, без повторного запроса уже данного разрешения.
