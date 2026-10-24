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

## Сохранённый checkpoint: 2026-10-24

- Проект: Drakengard 1, PS2 Europe. Опубликован `2cf7b2c` в `origin/main`.
- Реализованы EE INTC W1C, исправление ширины signed branches, рабочий PCH
  write-watch, IPU FIFO/нормальный DMA, BCLR/FDEC/SETIQ/SETVQ/SETTH,
  VDEC macroblock-address-increment, I/P/B-picture macroblock-type и motion code.
- Intra BDEC: MPEG2, IVF1, обычный scan, DC predictors, DC/AC VLC и escape,
  inverse quantization, математический IDCT, frame/field DCT, выходной FIFO.
  Bit-exact округление IDCT на PS2 не подтверждено. MPEG1, IVF0, alternate scan
  и non-intra BDEC пока явно отклоняются.
- Из IPU передаются настоящие 48 QW в scratchpad; адрес SPR не превращается
  в адрес основной RAM. Native216 подтвердил минимум 512 таких передач.
- Native217 измерил отказ `0x34000000`: CTRL=`0x02623F08`, PCT=2 (P-picture).
  Реализованы все семь P-MBT кодов, modes без поля длины, forward → MC_FRAME.
- Motion table2 реализована для0/±1..16. Signed DATA следует pinned reference;
  аппаратная упаковка отрицательных результатов не подтверждена. Invalid prefix
  даёт ECD без фиктивного motion=1.
- Native221 подтвердил две реальные `0x38000000`: DATA=`0x10000`,
  CTRL=`0x02623F08`, BP=`0x2086B` затем `0x2086C`.
- Native222 установил точный toSPR контракт: CHCR=`0x105` (source-chain),
  QWC=0, TADR=`0x006ACE80`, SADR=`0x70000000`, DMAE=1/suspend=0.
- Реализованы настоящие RAM→SPR source chains: REFE/CNT/NEXT/REF/END,
  TTE/TIE, 16-KiB wrap и DMAC cause9 после копирования. REFS/CALL/RET,
  VU/SPR sources и nonzero-QWC restart явно отклоняются. Normal SPR остаётся
  upstream; interleave/fromSPR chain не подменяются успешным завершением.
- RGB32 CSC реализована: planar Y/Cb/Cr384B → RGBA1024B на блок, целочисленная
  IPU matrix, clipping/SETTH и FIFO/backpressure. RGB16 явно отклоняется.
- Native224 прошёл `0x700003E8`: 1000 блоков, from-IPU QWC=`0xFA00`,
  RAM `0x01C4BB40..0x01D45B40`, FNV1a64=`0x21F997E470CAA325`
  (совпадает с начальным чёрным RGBA/alpha128). Exit6, `timed_out=false`.
- SPU2 ADMAS установлен: physical=`0x1F9001B0`, core0; core1=`0x1F9005B0`.
  Idle SH low bits0..3/readback реализованы в прежнем SpuQuiescentControl.
  Enable mask core0=1/core1=2; нет фиктивного переноса, STATX или IRQ.
- Native227 подтвердил retired ADMAS core0=1, ATTR=`0xC080`, STATX=0,
  затем отказал на реальном запросе **ADMA streaming transfer**: IOP PC=`0x88130`,
  DMA4 CHCR register=`0x1F8010C8`, requested CHCR=`0x01000201`,
  MADR=`0x0018C900`, BCR=`0x00C00010` (12288 bytes). Exit6/no timeout.
- Реальная ADMA уже реализована оставленным черновиком: split PCM16 в общей
  SPU RAM, opposite-half refill, MADR и completion по sample clock. Native229
  прошёл два refill и выдал PCM(-2,-2), затем остановился на nonzero FX guard.
- Native230 измерил фактические ненулевые FX коэффициенты и workspace
  `0xFC810..0xFFFFF`. Добавлен оригинальный scalar DSP: FIR39, reflections,
  comb/APF, signed Q15, actual delay RAM. FX-off сохраняет фильтры без RAM stores.
  Старые zero-input probes сохраняют свою отдельную строгую границу.
- Native231 прошёл эту границу: три refill по512 halfwords, реальный ненулевой
  PCM и минимум320 ненулевых delay stores. Весь stream ещё не завершился до
  следующего видеоотказа; host audio и native full-stream completion не доказаны.
- B-picture MBT реализован: все11 префиксов, DATA=(length<<16)|modes|MC_FRAME,
  включая intra; I/P без поля длины. D-picture остаётся явным отказом.
  Native232 прошёл минимум512 B-MBT, DATA0x40088, advancing BP/TOP. Также появились
  четыре настоящих ADMA completion (общие counters102..105), сверх refill-only231.
- Native232 timed_out300s без finalGS. Dump после остановки прогресса установил
  свёрнутое host-окно: CORE flags0xA04, ALWAYS_RUN=0, eventWaiting=0. Raylib ждёт
  восстановления окна. Stack candidates/MAP с точным совпадением .text —
  диагностика, не полноценный unwind; dump и MAP только в ignored work/.
- Для файлового --pad-control включён/проверяется FLAG_WINDOW_ALWAYS_RUN;
  гостевые данные/completion не менялись. Native233 — bounded проверка;
  фактический следующий контракт и номер продолжения записаны ниже.
  Не угадывай backward-motion/non-intra ветку до её наблюдения.
- Native225 timed out300s до movie, native226 crash=`0xC0000409` до нового
  ADMAS=1. Эти прогоны не доказывают контракт. В227 ProcDump crash не повторил,
  dump не создал; причина219/226 не установлена и не исправлена.
- Финальный GS231 чёрный640x508. Меню/nonblack movie/audio/gameplay/saves/
  recovery не подтверждены. Strict IOP1/1, bootstrap10/10, Python55, native
  Release/public-tree PASS. Изменения локальные; без commit/push/CI claim.

## Актуальный следующий блокер после234

Native233/234 exit6, timed_out=false. Реальный guest PC0x87778 очищает STR:
CHCR0x01000201→0x00000201 в0x1F8010C8, core0 ADMA. MADR/source0x18DD00,
BCR0x00C00010, end0x18F900, remaining3584 halfwords, pending0 bytes,
sample481783, IOP step375375262. Четыре ADMA completion до остановки повторены.
Финальный GS234 идентичен чёрному233; меню/host audio не подтверждены.

Следующий свободный номер235 (сначала проверь). Установи точный контракт
STR-clear для producer/refill, уже записанной ring RAM, MADR/BCR, busy/STATX,
consumer cursor, IRQ и rearm. Только затем реализуй отмену и stop/rearm tests,
сборку и bounded native. Не подменяй отмену finish()/ложным completion.
Подробности и private evidence pointers находятся в верхнем handoff.
Bootstrap10/10, strict IOP1/1, Python55 и native Release PASS. Все изменения
локальные, без публикации. Сплешскрин native/Splash.png сохранён и ждёт отдельной
интеграции после текущей device-границы.

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

На момент checkpoint следующий номер —232. Сначала проверь, что каталог
`work/native-run-232` свободен и другой native/reference процесс не работает.
Если номер уже занят, выбери следующий свободный, сохрани прежние результаты.

Настоящий executable: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`recomp_smoke.exe` не заменяет его. После изменения runtime собери настоящий
runner, проверь exit code сборки и запиши SHA-256 проверяемого executable.

```powershell
cmake --build build/native --config Release --target ps2EntryRunner --parallel 4
python scripts/run_native.py --input-complete --run 235 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 512
```

Пока launcher работает, отдельным процессом запусти:

```powershell
python scripts/native_start.py --run 235 --timeout 300
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
