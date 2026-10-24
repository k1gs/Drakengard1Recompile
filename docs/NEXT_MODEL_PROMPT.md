# Промпт для следующей модели

Продолжи Drakengard 1 Recompile, PS2 Europe, в `D:\proj\drakengard 1 recompile`.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний
актуальный docs/HANDOFF_2026-10-24_CONTINUATION.md. Проверь HEAD/status/процессы.
Входной checkpoint231fa5e пройден; текущий опубликованный hash смотри в Git.

Missing scheduler target0x298154 оказался обрезанным leaf map, не просто
пропущенным alias. Existing RecoverLeafFunctionFlow восстановил реальную функцию
0x297E44 до exclusive0x2983E8. Новая карта functions-ghidra-009.csv/fresh lift008
подключена runner overlay; старый generated/upstream/input не редактировались.
Локальный synthetic delay-slot-exit до recovery падает на missing epilogue;
после выполняет exit/busy-loop/pause-resume с правильными delay effects.
Python55, bootstrap11/11, strict IOP1/1, полный настоящий runner build PASS.

Native242 прошёл старую границу;243 измерил следующую: **normal toSPR DMA9 wait**
на PC0x2981E4/RA0x2984FC. CHCR0x101, MADR0x110049E0 (существующая VU0 data RAM),
QWC0x60, SADR0, D_CTRL1/D_ENABLER0x1201. STR остаётся установленным до300s watchdog.
Меню/Yes проходят обычным pad до200s; IOP/SIF продолжают работу. Последний
periodic GS16800 чёрный640x511, final-on-exit нет. Gameplay/audio/saves не доказаны.
VCALLMS после этого wait ещё не выполнен. Старые C0000409/keyCD277739 не исправлены.

Следующий244 (проверь номер): исследуй normal DMA9 admission/transfer/completion
для измеренного VU0 data source. Используй существующие shared VU0 data и SPR,
установи настоящие MADR/QWC/SADR/STR и completion/cause9 по pinned reference.
Реализуй подтверждённый контракт с поведенческими boundary/IRQ тестами,
собери настоящий runner и проверь bounded native1024M/300s. Не ограничивайся планом.

Не используй Computer Use/OS input, guest ready/PC/RA patches, skip, фиктивный
IRQ/перенос или нулевые данные. Startup helper unchanged; первые periodic capture
могут отсутствовать, не жди слепо файл600. Меню-рецепт и evidence в CONTINUE/handoff.
Сохрани .vscode/ и native/Splash.png: сплешскрин отложен. Кадры/дампы/inputs/
BIOS/generated остаются ignored, кроме уже approved README images. Субагенты
необязательны; разрешена только GPT-5.6 Luna. Focused validated commit/push разрешён
после staged review/check_public_tree.py. В конце обнови журнал, handoff,
CONTINUE и этот промпт; сообщи фактический результат и один следующий эксперимент.
