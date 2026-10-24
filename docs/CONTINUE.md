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

## Актуальный checkpoint: 2026-10-24

- Входной опубликованный checkpoint —58c9520. Текущий опубликованный hash
  определяй по Git HEAD/origin/main; focused validated commits/push разрешены.
- Реализованы I/P/B MBT/motion, toSPR chains, RGB32 CSC, sample-paced ADMA/FX,
  двухэтапный ADMA stop без фиктивного IRQ/completion/RAM clear.
- Native237 реально отрисовал главное меню и NORMAL/Yes/No. Native238 прошёл
  эти состояния раньше обычными кнопками и получил явный отказ IPU read.
- Native239 установил, что этот отказ вторичен: consumer0x13764C получает
  count0, его unsigned loop выходит из scratchpad и позже читает0x1000200C.
  Producer0x137D30 видит два объекта, grouping reference0/key0x7FFFFFFF.
- Причина первого установленного дефекта — scalar `C.LT` через host IEEE `<`:
  PS2 max sentinel0x7FFFFFFF становится host NaN. `ee_fpu_compare.h` и прежний
  build-local macro shadow теперь нормализуют операнды C.EQ/C.LT/C.LE по pinned
  PCSX2v2.8.2: exponent0→zero, exponent255→signed finite host max. Исходные FPR
  не переписываются; весь FPU arithmetic/rounding и hardware equivalence не заявлены.
- Native240 подтвердил исправленный путь: grouping reference0x3B05F0 и consumer
  count2 вместо0, два вызова. Новый реальный stop: **missing EE scheduler target
  0x298154**, RA0x2984FC, SP0x11D9D0. Exit5/timed_out=false — не успех игры.
- Проверенный настоящий runner SHA256:
  `26fab6b8e49ae69029683a14acd99950ae9a99ca48038a01bfd44d4ea45746a4`.
  Bootstrap11/11 (новые numeric boundary/consumer tests), strict IOP1/1,
  Python55 PASS. Native Release/MSBuild17.11.9 PASS.
- Финальный GS240 чёрный640x511. Gameplay/host audio/saves/recovery не доказаны.
  Во втором наблюдении grouping key0xCD277739 не объяснён; это не доказательство
  корректной геометрии. Crash219/226 также не объявлен исправленным.
- `.vscode/` и `native/Splash.png` сохраняй; сплешскрин отложен.

## Следующий эксперимент

Следующий241 (проверь, что свободен): установить контракт resume/dispatch для
реального scheduler PC0x298154. Найти owning lifted function и resumable labels,
сверить emitted checkpoints, генератор и registered-function table. Исправить
оригинальный генератор/overlay, если подтверждён пропущенный resume entry, затем
поведенческий yield/resume regression, настоящий runner и bounded native.
Не добавлять произвольный alias/stub, не исправлять RA/PC или guest flags.
Указатель может быть следствием предыдущей ошибки: не объявлять причину до сверки.

Повторить обычные меню с1024M/300s. В239 fully-lit titleGS9000 → Start,
menuGS10200 → Cross, dialogGS11400 → Left, highlighted YesGS12000 → Cross.
В240 title ещё fading на9000 и готов на9600; состояния проверяй, а не нажимай
вслепую по номеру кадра. Ранний Start238 во время fade был принят SIO2, но меню
не открыл; повтор на готовом титуле сработал. Startup helper unchanged.

Private evidence: `work/native-run-{238,239,240}/`, соответствующие
`work/post-menu-runN-launcher.log`, `work/fpu-compare-contract-240.json`,
`work/fpu-compare-reference-001/`. Подробности — верхний handoff.
Промпт продолжения — docs/NEXT_MODEL_PROMPT.md.

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

На момент checkpoint следующий номер —241. Сначала проверь, что каталог
`work/native-run-241` свободен и другой native/reference процесс не работает.
Если номер уже занят, выбери следующий свободный, сохрани прежние результаты.

Настоящий executable: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`recomp_smoke.exe` не заменяет его. После изменения runtime собери настоящий
runner, проверь exit code сборки и запиши SHA-256 проверяемого executable.

```powershell
cmake --build build/native --config Release --target ps2EntryRunner --parallel 4
python scripts/run_native.py --input-complete --run 241 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 1024
```

Пока launcher работает, отдельным процессом запусти:

```powershell
python scripts/native_start.py --run 241 --timeout 300
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
