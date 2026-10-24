# Промпт для следующей модели

Продолжи Drakengard 1 Recompile, PS2 Europe, в `D:\proj\drakengard 1 recompile`.
Прочитай AGENTS.md, docs/CONTINUE.md, последние CURRENT_STATE.md и верхний
актуальный docs/HANDOFF_2026-10-24_CONTINUATION.md. Проверь HEAD/status/процессы.
Опубликованный входной checkpoint58c9520 уже пройден; текущий hash смотри в Git.

Native238/239 установили post-Yes дефект scalar FPU compare: max sentinel
0x7FFFFFFF превращался в host NaN, grouping reference оставался0, consumer
0x13764C получал count0 и уходил из scratchpad; поздний IPU read0x1000200C —
следствие, не новый контракт регистра. Исправлен C.EQ/C.LT/C.LE в существующем
macro shadow с pinned reference normalization. Bootstrap11/11, strict IOP1/1,
Python55 и полный Release runner build прошли.

Native240 реально подтвердил grouping reference0x3B05F0/count2 и два вызова
consumer, затем остановился на **missing EE scheduler target0x298154**,
RA0x2984FC/SP0x11D9D0. Exit5/timed_out=false — не успех игры. Финальный GS
чёрный640x511. Меню/диалог отрисованы; gameplay/host audio/saves/recovery и
корректность всей геометрии/FPU arithmetic не доказаны. Второй key0xCD277739
не объяснён. Evidence/hashes/обычные меню-кнопки — в handoff и CONTINUE.

Следующий эксперимент241 (проверь свободный номер): установи owning function,
emitted checkpoint/resumable labels и registered dispatch contract для0x298154.
Не считай заранее, что это просто пропущенный alias. Исправь подтверждённый
генератор/overlay, добавь поведенческий yield/resume regression, собери настоящий
runner и проверь bounded native1024M/300s. Не ограничивайся планом.

Никаких Computer Use/OS input, готовых guest flags, skip-патчей, выдуманного
успеха или прямого изменения RAM. Inputs read-only; upstream/generated не
редактируй. Сохрани .vscode/ и native/Splash.png: сплешскрин отложен.
Кадры/дампы/BIOS/игровые данные/generated остаются private/ignored. Субагенты
необязательны; если нужны, разрешена только GPT-5.6 Luna. Разрешены focused
validated commits и обычный push после staged review/check_public_tree.py.
По завершении обнови журнал, верхний handoff, CONTINUE и этот промпт; сообщи
фактический результат, оставшуюся границу и один следующий эксперимент.
