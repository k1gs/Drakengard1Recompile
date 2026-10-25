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

## Актуальный checkpoint: продолжение главы I, 2026-10-25

Пользователь отложил FPS меню: продолжай ИГРУ. После обычного Start15 на первой
странице пролога255/256 реально отрисован "Chapter I - Beginnings",640x511.
Это новая граница, ещё не движение/бой/полный gameplay/audio/saves/recovery.
Старые missing EE targets исправлены; Timer1 bug не установлен.

255 exit6 до300s: исчерпан точно фиксированный1024M IOP allowance, а не найден
новый device fault. Добавлен явный2048M пресет (default256,512/1024 сохранены),
монотонный и без продления.32-bit overflow guard, bootstrap14/14, Python57 и
actual Release PASS.256 с2048M прошёл прежний предел и завершён watchdog300s,
exitnull. Поздний Cross16 на ChapterI имел SIO2 ACK, эффект сцены не проверен.

Следующий257 пока НЕ запускался: 300s/2048M, FRAME_INTERVAL300/FRAME_LIMIT64,
metricsOFF. Capturelimit — opt-in1..64, default32; parser/test/build проверены,
новая настройка64 native-проверки ещё не имеет. Текущий rebuilt EXE106577eb...
не запускался; последний реально проверенный256 был c312c31d.... Exact SHA в handoff.

Подготовлены private work/chapter1-257/{menu.py,start-prologue.py,chapter-input.py},
только syntax checked. Запусти startup helper после pad-control; остальные могут
ждать своей фазы параллельными процессами: menu257 проходит NewGame; prologue
ждёт menu finish и посылает Start на странице; chapter ждёт prologue finish,
посылает Cross на проверенном ChapterI и затем один Start, только если тот же
титул остаётся после600 UI observations. При смене ROI — остановка ввода и
визуальная проверка нового сырого кадра. ROI change/ACK не доказывают gameplay.

Один контроллер на фазу, actual SIO2 press/release. Каждый run максимум300s.
Никаких timer/ready/PC/RAM/movie patches, unlimited budget или auto-renewal.
Дойди до следующей реальной сцены/первого подтверждённого blocker; performance
сейчас отложен. .vscode/Splash сохраняй. См. верхний handoff и NEXT_MODEL_PROMPT.

## Сохранённый checkpoint пролога: 2026-10-25

- Все три подтверждённых truncated leaves восстановлены:297E44..2983E8,
  29322C..2937D0,297878..297E44. Последний имеет реальный JR297E3C/SP+60
  delay297E40,1484 actual bytes,+388 coverage. Нет aliases/пустых returns.
- Fresh map011/lift010 сохраняет старые исправления и unrelated rows. Все36
  resume labels зарегистрированы.8578compiled/0stubs/skips/decodefailures.
  Fresh Ghidra behavioral regression и full real Release/PCH build PASS.
- Native246/247/248 на одном binary проходят прежний297BB0 и выполняют хвост
  с реальным VU0-data4860→SPR transfer. Явных новых scheduler/device отказов нет.
- Новый визуальный milestone: после New Game появилась ПЕРВАЯ ТЕКСТОВАЯ СТРАНИЦА
  ПРЕДЫСТОРИИ, GS640x511,246frame15600/248frames14400,15000, приватно просмотрены.
  Это ещё не gameplay/host audio/saves/recovery. Все прогоны300s watchdog,
  timed_out=true/exit=null, IOP/SIF/callback processing активны. Нет final-on-exit GS.
- Чистый ввод246: Start11/Cross12/Left13/Cross14, все SIO2 press/release.248 так же,
  затем один Cross15 на подтверждённом прологе (SIO2 ACK). После него текст страницы
  тот же; пиксели отличаются. Семантический переход страницы НЕ подтверждён.
- Старый ROI oracle в247 поздно признал корректный диалог; это не отказ игры.
  Private248 oracle проверяет menu MAE2, dialog text/buttons15/8 и prologue
  normalizedROI15. Это checkpoint-specific detection; raw evidence не изменено.
- Точные hashes/ограничения/evidence — верхний handoff. Source input/upstream/
  old generated/maps не менялись. .vscode/Splash сохрани; сплешскрин отложен.

## История: прежняя граница пролога

Исторически границей была первая страница пролога; теперь продолжай главу I выше,
а НЕ missing scheduler297BB0. Установи ожидаемое поведение страницы: ordinary
Start/Cross после проверки готовности; реальный callback/input handler и, при
необходимости, read-only Timer1 COUNT/MODE/софт-прогресс. Повторяющийся PC27F1B8
и getter1215F4(10000800) не доказывают timer bug. При необходимости сравни
эквивалентное состояние owned reference, последовательно с native.

Не форсируй ready/time/PC/RAM. Пройди следующую реальную сцену и проверь движение/
бой прежде, чем заявлять gameplay. Каждый native/reference run максимум300s.
Private248 menu script accepts run ID; prologue script hardcodes248/output/seq14,
поэтому используй новую копию с проверенными именами/sequence, не overwrite.
Один controller за раз, actual GS state и SIO2 press/release; ACK не равен UI action.

Для будущего полного Ghidra export из read-only проекта повтори ВСЕ ТРИ verified
recoveries выше, чтобы не регрессировать coverage. Workflow в ANALYSIS_REGRESSIONS.
Old maps/lifts сохраняй; сначала проверь новые имена. Промпт — NEXT_MODEL_PROMPT.

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

На момент checkpoint следующий номер —257. Сначала проверь, что каталог
`work/native-run-257` свободен и другой native/reference процесс не работает.
Если номер уже занят, выбери следующий свободный, сохрани прежние результаты.

Настоящий executable: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`recomp_smoke.exe` не заменяет его. После изменения runtime собери настоящий
runner, проверь exit code сборки и запиши SHA-256 проверяемого executable.

```powershell
cmake --build build/native --config Release --target ps2EntryRunner --parallel 4
$env:DRAKENGARD_FRAME_INTERVAL="300"
$env:DRAKENGARD_FRAME_LIMIT="64"
python scripts/run_native.py --input-complete --run 257 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 2048
```

Пока launcher работает, дождись появления `work/native-run-257/pad-control.txt`,
затем отдельным процессом запусти:

```powershell
python scripts/native_start.py --run 257 --timeout 300
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
`DRAKENGARD_FRAME_INTERVAL=600`: это opt-in прямые GS-снимки с default-пределом32 файла. Opt-in `DRAKENGARD_FRAME_LIMIT=1..64`
задаёт другой конечный предел; для следующего257 выше указаны300/64.
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
