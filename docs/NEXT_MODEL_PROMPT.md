# Промпт для следующей модели

Продолжи Drakengard 1 Recompile, PS2 Europe, в `D:\proj\drakengard 1 recompile`.
Сначала прочитай AGENTS.md, docs/CONTINUE.md, последние записи CURRENT_STATE.md
(локальный ignored журнал) и верхний актуальный раздел
docs/HANDOFF_2026-10-24_CONTINUATION.md. Проверь Git HEAD/status и реальные процессы.

Checkpoint: реализованы IPU I/P/B MBT/motion, toSPR chains, RGB32 CSC, ADMA/FX,
двухэтапный ADMA stop (STR-clear, затем ADMAS0) без ложного completion/IRQ/RAM clear.
Native237 реально отрисовал главное меню New Game / Load Game / Options и диалог
NORMAL. Обычные Start/Cross/Left/Cross приняты SIO2. После New Game Yes последний
GS чёрный; прогон завершён watchdog300s при продолжающемся IOP/SIF. Новый device
отказ не установлен. Gameplay, host audio, saves/recovery пока не доказаны.

Следующий эксперимент238 — сначала проверь, что номер свободен. Повтори с1024M
IOP budget и прежним лимитом300s. Пройди уже проверенные GS-состояния обычным host
pad protocol быстрее, оставив больше времени после New Game. Затем установи и
исправь первую подтверждённую post-Yes границу; не угадывай причину чёрного кадра.
Команды launcher/helper/input и evidence pointers находятся в docs/CONTINUE.md
и handoff. Не ограничивайся планом: реализуй установленный контракт, добавь
поведенческие проверки, собери настоящий runner и проверь bounded native run.

Не используй Computer Use/OS input, готовые guest flags, skip-патчи, фиктивный
успех или прямые изменения RAM. Inputs read-only; upstream/generated не редактируй.
Сохрани .vscode/ и native/Splash.png — сплешскрин отложен. Кадры/дампы/game/BIOS
и generated не публикуй. Субагенты необязательны; если нужны, разрешена только
GPT-5.6 Luna. Разрешены focused validated commits и обычный push после проверки
staged diff и scripts/check_public_tree.py. По завершении обнови журнал, верхний
handoff и CONTINUE, сообщи фактический результат и дай новый промпт продолжения.
