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

- Entry e1214d3 содержит проверенный normal VU0-data→SPR DMA9. Текущий HEAD
  определяй по Git; unrelated .vscode/Splash сохраняй.
- Восстановлен следующий leaf29322C: настоящий JR2937C8 и SP+60 delay2937CC,
  exclusive bound2937D0,1444 actual bytes. Три фрагмента объединены,+388 bytes.
  Prior297E44 recovery и все unrelated map rows сохранены. Никаких aliases.
- Fresh map010/lift009,8580 compiled/0stubs/skips/decodefailures. Все12 resume
  labels29322C в switch и dispatch. Runner overlay использует lift009; old008
  не редактировался. Fresh Ghidra pause/resume/delay regression и Release/PCH PASS.
- Native245 реально проходит29353C/owner29322C и продолжает до НОВОГО missing
  scheduler target **0x297BB0, RA0x2984B4**, owner0x297878. Exit5/no timeout.
  Последний GS640x511 flatRGB128 gray, gameplay/audio/saves/recovery не доказаны.
- У owner297878 карта заканчивается297BB0 после alternate-entry jump-delay.
  Adjacent fragments297D20..297D40/297D54..297E44. Это гипотеза ещё одного
  обрезанного leaf; реальный epilogue и bound сначала подтвердить по инструкциям.
- Input245 не минимальный baseline: observer подал Start11/Cross12/Left13 с
  SIO2 ACK, затем был прерван, extraStart14 ACK. Cross15 не подтверждён SIO2.
  Не делай причинных выводов по stale image/задержке вывода. Оставляй одного
  observer; сначала проверяй его JSONL и события/ACK, потом вмешивайся.
- Exact hashes/evidence/границы проверки — верхний handoff и CURRENT_STATE.md.

## Следующий эксперимент

Следующий246 (проверь unused): original instructions/real epilogue owner297878,
проверенный bounded recovery и свежий map/lift, resume audit, behavioral tests,
real Release/PCH и bounded native246. Соседний map fragment не доказывает bound.

Ghidra project read-only: перед новым полным экспортом нужно повторно применить
ОБА сохранённых recovery297E44..2983E8 и29322C..2937D0, плюс новое подтверждённое.
Не регрессируй старую coverage. Workflow — docs/ANALYSIS_REGRESSIONS.md; предполагаемые
fresh map011/lift010 сначала проверь на отсутствие. Old maps/lifts не перезаписывай.

Ordinary menus: наблюдай actual GS title/menu/NORMAL/highlighted Yes, подавай
Start/Cross/Left/Cross700ms с actual SIO2 press/release. Private observer
work/spr-vu0-menu-244.py принимает новый run ID; reference ROIs checkpoint-specific.
Первый capture600 может отсутствовать. Переиспользуй код с новым output, а не
старый JSONL. Промпт продолжения — docs/NEXT_MODEL_PROMPT.md.

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

На момент checkpoint следующий номер —246. Сначала проверь, что каталог
`work/native-run-246` свободен и другой native/reference процесс не работает.
Если номер уже занят, выбери следующий свободный, сохрани прежние результаты.

Настоящий executable: `build/native/ps2xRuntime/Release/ps2EntryRunner.exe`.
`recomp_smoke.exe` не заменяет его. После изменения runtime собери настоящий
runner, проверь exit code сборки и запиши SHA-256 проверяемого executable.

```powershell
cmake --build build/native --config Release --target ps2EntryRunner --parallel 4
python scripts/run_native.py --input-complete --run 246 --seconds 300 --iop-bootstrap --ee-firmware-entry --iop-video-events --virtual-pad --pad-control --iop-runtime-millions 1024
```

Пока launcher работает, отдельным процессом запусти:

```powershell
python scripts/native_start.py --run 246 --timeout 300
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
