# Источники и инструменты

Проверено 2026-10-16. Ссылки меняются; перед интеграцией сохранить точный commit/версию.
Здесь нет скачивания игры, BIOS или готовых игровых артефактов.

| Источник | Для чего | Ограничение |
| --- | --- | --- |
| [PCSX2 Drakengard entry](https://wiki.pcsx2.net/index.php?title=Drakengard) | Europe serial SLES-52322, эталонные сведения | Не подтверждает ревизию нашей копии |
| [PCSX2 source](https://github.com/PCSX2/pcsx2) | Проверка поведения EE/IOP/VU/GS, debugger/reference | Эмуляторный запуск не доказывает native-порт |
| [PS2Recomp](https://github.com/ran-j/PS2Recomp) | Candidate static R5900→C++ и runtime | Upstream обозначает experimental; не проверен на Drakengard |
| [PS2Recomp analyzer/Ghidra workflow](https://github.com/ran-j/PS2Recomp/tree/main/ps2xAnalyzer) | Карта функций stripped/retail ELF | Экспорт должен соответствовать точному ELF |
| [PS2Recomp license](https://github.com/ran-j/PS2Recomp/blob/main/LICENSE) | Лицензирование будущего fork/runtime | Отдельная проверка перед заимствованием/распространением |
| [Ghidra](https://github.com/NationalSecurityAgency/ghidra) | Static ELF/function analysis | Проверить поддержку R5900/MMI выбранным processor module |
| [ps2tek](https://psi-rockin.github.io/ps2tek/) | Техническая справка по PS2 | Сверять критичные contracts с наблюдением и исходниками |
| [ELF specification](https://refspecs.linuxfoundation.org/elf/elf.pdf) | Структура заголовков ELF и PT_LOAD | Не заменяет PS2 ABI и анализ конкретной игры |

PS2Recomp рассматривается как отправная точка. Его README описывает analyzer, translator,
runtime и IOP модуль, но также предупреждает о частичном hardware coverage. До входного ELF
невозможно честно оценить объём game-specific работы или обещать playable результат.

## Toolchain register

- Подготовительные scripts: Python 3.10+, standard library, Git.
- Предполагаемый native build: CMake 3.20+, C++20 compiler, первоначально MSVC x64.
- Перед M2: проверить upstream требования заново, выбрать commit, записать compiler/SDK,
  CMake и зависимости. Не использовать плавающий main как воспроизводимый lock.
- PCSX2/Ghidra/PS2Recomp сейчас не скачаны, не собраны и не включены в репозиторий.
