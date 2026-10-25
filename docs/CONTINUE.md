# Продолжение работы

## Current checkpoint: Continue accepted; scene transition under investigation (2026-10-25)

User resumed work. No subagents/Computer Use/OS input. Menu FPS work remains
postponed; continue real game progression. Earlier STOP and pending sections
below are historical and superseded by this checkpoint and CURRENT_STATE.

Native278 repaired the private menu observer: fresh confirmed No6120 -> ordinary
Left16 -> actual Yes6300 -> Cross17, with serial press/release ACKs. Classifier
rejects fading/unknown images; at most3 Left retries, only on fresh confirmed No.
Retry branch was not needed live. Offline all275 frames of277:0 false Yes.

Continue readiness is now NATIVE VERIFIED: briefing14880, real watch4fbd30
state2->3, one Cross23/3000ms, then original consumer PC2c692c wrote3->4.
This proves acceptance of Continue, not world/gameplay. Last278frame16140 black;
300s watchdog,267 captures, no target789read, no new explicit runtime refusal.
No dump: the post-read trigger was never reached. Startup/intro/menu0;
progression1 at295s and observer1 are bounded observation limits.

Focused source changes: optional capture cap1..320/default32 unchanged; capture
configuration log; shared fixed-size chunk P6 writer replaces per-pixel stream
calls in all3 capture paths. Header, RGB bytes, alpha discard, existing selection
and no-overwrite behavior preserved. No guest/timer/renderer arithmetic changes.
Synthetic17/17 tests PASS including exact PPM bytes across4096pixel boundaries;
actual Release build PASS. Synthetic /Od file benchmark:10x640x511 writes,
3alternating pairs, median0.0967087s legacy vs0.0240027s current,981135bytes equal.
This is diagnostic serialization overhead only, not measured game FPS.
New runner SHA0d24cede033b00c545772545212b273d56994514e03b794834285d6c6db397f1.
Native279 completed300s/2048M/interval60/cap320/metricsOFF/watch4fbd30:
274periodic frames,last16560 black. Startup/intro/menu0,progression1(deadline295s),
observer1/no dump. OneCross23 waitedUI3; actualPC2c692c again wrote3->4.
No target789read or new explicit runtime refusal. No gameplay proof.
Native280 completed same300s profile,watchdogtrue/exitnull,271captures,last16380
still a darkened briefing (not a world). Startup/intro/menu0,progression1 at295s.
OneCross23 accepted. Observer0 collected ONE owned-process snapshot after exact
PC2c692c WRITE32 UI3->4:701889699bytes,794Memory64 regions structurally complete;
collector exit1 recorded truthfully. Runtime byte offsets4032187->4045688.
Snapshot shows UI4/enable1,fader current20,target255,remaining55 of60steps.
IOP964931584; old resource slot status3/job19decimal,LSN867318/dest491c00/131072B,
NOT target789. No target1485718/1485920read. This is still closing the briefing,
not evidence of lost DMA or a failed scene load. Main PC2fdd08/RA28c0a8/SP11ff00;
original2fdd08 is a VIF1DMA CHCR store launching work, not a spin-loop diagnosis.
All native/collector processes stopped. Next281 unused (recheck before launch).

Scene request273 remains the causal baseline: scene143194/state2 waits resource
file789,LSN1485718,481792bytes,destb7db40, status3b2ea0=1/job35 at that snapshot.
Its actual IOP1182076928 PRECEDED DMA1182375149. Old destination bytes therefore
DO NOT prove lost DMA. After actual final targetread1485920, require fresh image
and one owned-process read-only snapshot with runtime byte offsets and independent
IOP counter. Job IDs are dynamic; older job19hex completion is not target proof.
Private273/report-snapshot.py validates main context/BareCallState; revalidate
layouts after toolchain changes. All dumps/frames/inputs/controllers stay ignored.
Preserve unrelated .vscode/, native/Splash.png, upstream/generated/maps.
Closing path reconfirmed from the verified original ELF:2c6900 ->2775e0 sets
fader48a42c,target255,steps60. State4 at2c6930 calls341804 (lw8(a0)) and waits
remaining48a434; zero advances UI0x12 at2c6950. A black frame may precede this
completion. Snapshot280 confirmed remaining55, so the target scene request was not active yet.
No success stubs, PC/RAM/ready/timer/movie patches or budget renewal.

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

Номер сверяй с верхним checkpoint, каталогом work/ и реальными процессами.
Пример ниже рассчитан на следующий281, если он ещё свободен;280 сначала прочитай до конца. После изменения
runtime сначала собери настоящий runner, проверь exit code и SHA-256; smoke.exe
не заменяет ps2EntryRunner. Сборка при живом native процессе запрещена.

```powershell
$nativeRun=281
$env:DRAKENGARD_FRAME_INTERVAL="60"
$env:DRAKENGARD_FRAME_LIMIT="320"
python scripts/run_native.py --input-complete --run $nativeRun --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 2048 --ee-write-watch 0x4fbd30
```

Для обычного ввода используй новую private копию последнего controllers.py с
правильным run ID. Не жди конца launcher: controller должен дождаться реального
pad-control.txt и запустить native_start --timeout300 --attempts5 --after-frame1200.
Последний1200 откалиброван actual271; более старые214–216 имели другое состояние.
260/intro.py принимает run CLI;279/menu.py запускается ТОЛЬКО после intro0;
279/progression.py принимает run CLI, ждёт menu.log completion marker и UI state3.
Один controller на фазу; корректные startup.log/intro.log/menu.log обязательны.

DRAKENGARD_FRAME_LIMIT1..320 — opt-in конечный cap, default32. Интервал60..3600
измеряется в host UI observations, не игровых кадрах/FPS. Interval60 даёт более
частое распознавание состояния и добавляет capture I/O overhead. Сохраняй actual
config line, EXE hash и число файлов в evidence. Ни rawGS, ни metadata не заменяют
проверку gameplay. Native/reference последовательно, каждый максимум300s.

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
