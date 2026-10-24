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

- Входной опубликованный checkpoint —231fa5e (README с настоящим главным меню).
  Текущий опубликованный hash определяй по Git HEAD/origin/main.
- Сохраняются IPU I/P/B MBT/motion, toSPR chains, RGB32 CSC, ADMA/FX/stop,
  scalar COP1 compare fix. Неподдерживаемое поведение не подменяется успехом.
- Native240 missing scheduler target0x298154 был **обрезанным leaf map**, не
  основанием для произвольного resume alias. У функции0x297E44 анализ кончался
  после delay slot другого jump; обычное продолжение не было декодировано.
- Прежний RecoverLeafFunctionFlow.java восстановил реальные инструкции до
  exclusive0x2983E8, включая JR/stack restore. Новая карта functions-ghidra-009.csv:
  8583 records вместо8585, два старых фрагмента объединены, +388 реальных bytes.
  Прочие строки карты не изменились; исходный Ghidra project открывался read-only.
- Настоящий runner теперь использует свежий lift-008. Исходные inputs,
  upstream и прежние generated units не редактировались; новых stubs/skips нет.
- Синтетический delay-slot-exit regression: до recovery отсутствует epilogue,
  после — непосредственный выход, два busy iterations, pause/resume и реальные
  эффекты branch/jump/return delay instructions дают ожидаемые регистры.
  Это локальный Ghidra test, не полная проверка native scheduler.
- Native242 реально прошёл старую границу и выполняет восстановленный хвост.
  Следующая фактическая граница — ожидание toSPR DMA9 на PC0x2981E4,
  RA0x2984FC. Native243 измерил CHCR0x101/MADR0x110049E0/QWC0x60/SADR0,
  D_CTRL1/D_ENABLER0x1201. Source попадает в существующую VU0 data memory.
- Native242/243:1024M/300s, watchdog timed_out=true/exit=null; IOP/SIF работают,
  STR остаётся установленным. Финальный periodic GS16800 чёрный640x511.
  Нет final-on-exit capture, gameplay/host audio/saves/recovery не доказаны.
- Проверенный runner243 SHA256:
  `2052fa9845d1996478187657580203c590b2caf29cd785495164e76432c010bf`.
  Release/MSBuild17.11.9, Python55, bootstrap11/11, strict IOP1/1 PASS.
- Native241 не проверил post-Yes: throwaway observer ждал отсутствующий первый
  periodic capture600 и пропустил титул. Это не device fault; рецепт исправлен
  в242. Первая периодическая точка может не создать файл до готовности GS.
- `.vscode/` и `native/Splash.png` сохраняй; сплешскрин отложен. Старые
  C0000409 и grouping key0xCD277739 не объявлены исправленными.

## Следующий эксперимент

Следующий244 (проверь свободный номер): установить нормальный DMA9 контракт для
измеренного source0x110049E0 в VU0 data RAM, QWC0x60, destination SPR0.
Проверить admission, настоящий shared-memory producer, перенос, обновление
MADR/QWC/SADR/STR и completion/cause9 по pinned reference. Реализовать только
подтверждённое поведение в оригинальном адаптере; tests/build/bounded native.
Никакого STR-clear-on-read, фиктивного IRQ, отдельной RAM или нулевых данных.
Не заявлять, что VCALLMS после этой точки уже выполнен: native до него не дошёл.

Меню242/243: проверенный титулGS9000→Start, menu9600→Cross, NORMAL10200→Left,
highlighted Yes10800→Cross;700ms и подтверждённый SIO2 press/release. В243 Yes
подан около196.5s, остаётся примерно100s после него. Проверяй актуальные GS
состояния, не нажимай вслепую по номеру кадра. Startup helper unchanged.

Private evidence: work/leaf-flow-contract-243.json, work/native-run-{241,242,243}/,
work/flow-runN-{launcher,startup}.log и menu.jsonl; Ghidra/lift logs в handoff.
Восстановление карты и synthetic reproduction — docs/ANALYSIS_REGRESSIONS.md.
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

На момент checkpoint следующий номер —244. Сначала проверь, что каталог
`work/native-run-244` свободен и другой native/reference процесс не работает.
Если номер уже занят, выбери следующий свободный, сохрани прежние результаты.

Настоящий executable: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`recomp_smoke.exe` не заменяет его. После изменения runtime собери настоящий
runner, проверь exit code сборки и запиши SHA-256 проверяемого executable.

```powershell
cmake --build build/native --config Release --target ps2EntryRunner --parallel 4
python scripts/run_native.py --input-complete --run 244 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 1024
```

Пока launcher работает, отдельным процессом запусти:

```powershell
python scripts/native_start.py --run 244 --timeout 300
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
