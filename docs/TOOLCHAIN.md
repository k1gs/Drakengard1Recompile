# Toolchain checkpoint: 2026-10-16

## Проверенные версии

| Компонент | Версия / commit |
| --- | --- |
| PS2Recomp | `eb8688b2fd191129f103a39543002c005b33409c` |
| MSVC x64 | 19.41.34123, VS 2022 Community 17.11.5 |
| Windows SDK | 10.0.22621.0 |
| CMake | 3.31.0-rc1 |
| Python | 3.10.0 |
| Java | Oracle JDK 21.0.9 |
| Ghidra | 12.1.4 PUBLIC, revision `8b6bbb857accdfa20dc5b2f5dea471178c2e9fbc` |
| Emotion Engine Reloaded | v2.1.38, asset for Ghidra 12.1.4 |
| PCSX2 reference | v2.8.2 Windows x64 Qt portable |

Зависимости analyzer/recompiler, реально использованные в build:

| Библиотека | Tag | Commit |
| --- | --- | --- |
| ELFIO | Release_3.12 | `8ae6cec5d60495822ecd57d736f66149da9b1830` |
| toml11 | v4.4.0 | `be08ba2be2a964edcdb3d3e3ea8d100abc26f286` |
| fmt | 12.1.0 | `407c905e45ad75fc29bf0f9bb7c5c2fd3475976f` |
| libdwarf | v2.2.0 | `2e088b983562860f497b4fd789a3e61524eb857c` |
| rabbitizer | 1.14.3 | `d5804fa488474830a47fbf9f3693048ec1e45589` |
| nlohmann/json | v3.11.3 | `9cca280a4d0ccf0c08f47a99aa71d1b0e52f8d03` |
| raylib (native host) | 5.5 | `c1ab645ca298a2801097931d1079b10ff7eb9df8` |

## Analyzer/recompiler build

Исходники и dependencies находятся только в ignored tools/ и build/.
Для нового checkout получить перечисленные tags в соответствующие local directories и
сверить commit. PS2Recomp требуется checkout именно указанного commit, а не текущего main.
Для библиотек достаточно shallow clone без recursive submodules. У toml11 recursive clone
тянет ненужные test/docs зависимости; их для нашей сборки не использовали.

Команда, соответствующая локальной сборке (из корня, PowerShell):

```powershell
$root = (Get-Location).Path.Replace('\','/')
$argsForCMake = @(
  '-S','tools/ps2recomp','-B','build/ps2recomp-tools',
  '-G','Visual Studio 17 2022','-A','x64',
  '-DPS2X_BUILD_RUNTIME=OFF','-DPS2X_BUILD_TEST=OFF','-DPS2X_BUILD_STUDIO=OFF',
  "-DFETCHCONTENT_SOURCE_DIR_TOML11=$root/build/ps2recomp-tools/_deps/toml11-src",
  "-DFETCHCONTENT_SOURCE_DIR_FMT=$root/tools/deps/fmt",
  "-DFETCHCONTENT_SOURCE_DIR_LIBDWARF=$root/tools/deps/libdwarf",
  "-DFETCHCONTENT_SOURCE_DIR_RABBITIZER=$root/tools/deps/rabbitizer",
  "-DFETCHCONTENT_SOURCE_DIR_NLOHMANN_JSON=$root/tools/deps/json"
)
cmake @argsForCMake
cmake --build build/ps2recomp-tools --config Release --target ps2_recomp ps2_analyzer --parallel 4
```

Путь TOML11 выше — уже полученный локальный checkout. На чистой машине его можно заменить
на отдельный shallow checkout v4.4.0. ELFIO исходная CMake configuration получает автоматически.
Команды Windows-native Java/MSBuild в restricted token зависали до useful execution;
повторение без restricted token прошло. Это особенность этой среды, не ошибка guest ELF.

## Ghidra map

Официальные archives проверены по GitHub asset SHA-256 до извлечения; пути внутри архивов
проверены на absolute/parent entries и symlinks. Destination directories были новыми.

| Архив | SHA-256 |
| --- | --- |
| Ghidra 12.1.4 | `ddac49f903da9d5bac833e5cc79395098b9c33cfd3279be5f31bd00387d2d4db` |
| Emotion Engine Reloaded v2.1.38 / 12.1.4 | `e389ff3e921d9f7d4db2ece9906f808545ce4ff519d9e25a019b72ea509f240b` |
| PCSX2 v2.8.2 portable | `7dfc829ca1994cc1045ac49f05e39b6cf968b72e6a374c40e05c2a2b4ac200b4` |

Ghidra и plugin установлены в tools/ghidra_12.1.4_PUBLIC/. Все settings/cache/projects локальны.

```powershell
$env:JAVA_HOME = 'C:\Program Files\Java\jdk-21' # фактический путь JDK этой машины
$env:PATH = "$env:JAVA_HOME\bin;$env:PATH"
$env:XDG_CONFIG_HOME = "$PWD\work\ghidra-settings"
$env:XDG_CACHE_HOME = "$PWD\work\ghidra-cache"
# Сначала создать эти каталоги и work/ghidra-projects; имя проекта/CSV должно быть новым.
& tools/ghidra_12.1.4_PUBLIC/support/analyzeHeadless.bat `
  work/ghidra-projects drakengard-eu-001 `
  -import work/extracted-001/SLES_523.22 -processor r5900:LE:32:default `
  -scriptPath scripts/ghidra -preScript SeedLinkonceFunctions.java `
  -postScript ExportRecompMap.java "$PWD/work/functions-ghidra-002.csv" `
  -analysisTimeoutPerFile 600 -max-cpu 4
```

Seed script создал 1 346 function seeds из executable linkonce sections; analysis занял
66 секунд и обнаружил 7 796 functions. Исправленный exporter разбивает union их реальных
AddressRanges на 8 212 disjoint code records, покрывающих 2 169 136 байт, без заполнения дыр.
Полные addresses/analysis exports остаются
в work/. Map не является доказательством правильности всех функций и indirect targets.
Plugin сообщил, что DVP overlay blocks не byte-mapped; VU/DVP анализ ещё не завершён.

## Synthetic smoke и первый lift

```powershell
python scripts/make_smoke_fixture.py
& build/ps2recomp-tools/ps2xRecomp/Release/ps2_recomp.exe work/smoke-001/config.toml
cmake -S native/smoke -B build/smoke -G 'Visual Studio 17 2022' -A x64
cmake --build build/smoke --config Release
& build/smoke/Release/recomp_smoke.exe
python scripts/lift.py --input-complete --run-id lift-002
```

Fixture generator не перезаписывает существующий work/smoke-001/. Lift проверяет ELF SHA-256,
точный upstream commit и чистоту source checkout, требует map в work/ и новый run-id.
Map validator отклоняет overlaps, size/range mismatch и адреса вне executable PT_LOAD.
Это защищает от случайного использования другой ревизии, но ещё не проверяет происхождение
каждой строки map. `stubs=[]`, `skip=[]`, все три `patch_*` выключены.

Smoke действительно скомпилирован и исполнен: addiu выставил v0=42, jr перешёл на ra,
zero/s0 сохранены. Это проверка одного маленького фрагмента, не всего R5900/runtime.

Исторический lift-001: exit 0; 7 796 recompiled functions, 0 skipped/stubs/decode failures,
198 629 resumable entry points, 1 498 unresolved-indirect warnings с 357 922 fallback entries.
Generated files: 7 799, суммарно 162 739 681 байт. Компиляторный отчёт не подтверждает
корректность карты или native execution. Lift-001 признан непригодным для build: exporter
захватывал holes между частями тела функций, породив MSVC C1002 на двух 10-МБ generated functions.

Исторический lift-002: 8 212 fragments, 120 331 resumable entries, 1 536 warnings / 200 436 fallback
entries, 0 skipped/stubs/decode failures/errors. 8 215 generated files, 134 875 411 байт.
Отдельный synthetic Ghidra regression создал тело из [0x1000,0x1004) и [0x1008,0x100c):
exporter сохранил ровно эти два участка, 8 covered bytes, не включив 4-byte hole.
Это не доказательство корректности каждого indirect target или gameplay.

## Diagnostic native runner

Before native configure/build, prepare the kernel image and lift in [KERNEL_PATCH.md](KERNEL_PATCH.md).
The native loader still receives the original boot ELF; the wrapper ELF is only a translation input.

Текущий overlay использует `generated/lift-005`, полученный командой
`python scripts/lift.py --input-complete --map work/functions-ghidra-006.csv --run-id lift-005`.
Для повторения используйте свежие output IDs и согласуйте generated path в overlay.
Карта содержит только initialized executable memory Ghidra; upstream дополнительно исключил
кандидат из `.vutext`. 8 338 EE fragments, 120 416 resumable entries; 0 stubs/skips/decode failures,
1 540 indirect warnings. Восстановление callback entries не подтверждает полноту карты.

`SeedAddressFunctions.java` принимает подтверждённые runtime-адреса; `SeedReferencedCode.java`
исследует ещё не покрытые executable destinations существующих data references. Полученные
кандидаты требуют проверки: анализ может принять таблицу или VU payload за EE instructions.

`native/runner/overlay.cmake` добавляет наш host entry и локальные generated files к pinned
upstream targets через CMAKE_PROJECT_TOP_LEVEL_INCLUDES, не меняя tools/ps2recomp.
Unknown syscall fallback превращается в exception в private build-tree source; если точный
контекст upstream изменится, configure остановится. Guest skip/return-success не добавляются.
SetSyscall также отказывает в установке handler без generated code; диагностический prefix
и совпадения в RAM пишутся только в локальный лог и не создают execution aliases.
Runner проверяет ELF SHA-256 самостоятельно через Windows BCrypt, до runtime initialization.
На других платформах verifier отказывает; Python/CI tooling остаётся переносимым.
Scheduler snapshots читаются через штатный snapshot API. Счётчик diagnostic frames отражает
host render loop, а не количество отрисованных кадров игры.

```powershell
$root = (Get-Location).Path.Replace('\','/')
$nativeArgs = @(
  '-S','tools/ps2recomp','-B','build/native','-G','Visual Studio 17 2022','-A','x64',
  '-DPS2X_BUILD_RECOMP=OFF','-DPS2X_BUILD_ANALYZER=OFF','-DPS2X_BUILD_TEST=OFF',
  '-DPS2X_BUILD_STUDIO=OFF','-DPS2X_BUILD_RUNTIME=ON','-DPS2X_ENABLE_DEBUG_UI=OFF',
  '-DPS2X_ENABLE_AGRESSIVE_LOGS=OFF','-DPS2X_ENABLE_RUNTIME_LOGS=ON',
  "-DFETCHCONTENT_SOURCE_DIR_RAYLIB=$root/tools/deps/raylib-minimal",
  "-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=$root/native/runner/overlay.cmake"
)
cmake @nativeArgs
cmake --build build/native --config Release --target ps2EntryRunner --parallel 4
python scripts/run_native.py --input-complete --seconds 20 --run 1
```

raylib-minimal использует sparse checkout `src cmake`; full assets/examples не нужны.
FFmpeg включён upstream default: ExternalProject получает `n7.1-241205` LGPL amd64 shared
prebuilt из [upstream release](https://github.com/System233/ffmpeg-msvc-prebuilt/releases/tag/n7.1-241205).
Не выключать FFmpeg ради продвижения по видео: upstream disabled path выдаёт stub frames.
Runner использует /Od для первого generated-code bring-up и /fp:precise, не performance build.
Generated units компилируются /MP4; это ограничение compiler workers, отдельно от MSBuild --parallel.
Полученный локально FFmpeg archive SHA-256:
`b985887dbca8c0a9a4c47f9d2bac7ec3ab994335b1b7fe7000bdb1dcd772ac68`.
Это fingerprint скачанного архива; GitHub asset digest для старого релиза отсутствовал.
Запуск ограничен supervisor timeout; logs и executable fingerprint сохраняются в work/.
Бинарник не распространяется. Upstream PS2Recomp имеет GPLv3; собственный MIT glue не меняет
лицензию linked runtime или private generated replacement source.

## Reference status

Изолированный PCSX2 распознал serial SLES-52322, version 1.00, CRC 79585776, Europe BIOS 1.60
и создал D3D12 swapchain. Startup с forward-slash ISO path не прошёл FileExists; повторение
с native Windows paths открыло диск. Собственный профиль и новые memory cards находятся
в work/pcsx2-reference/; пользовательский профиль не изменялся, BIOS скопирован локально.

Hidden baseline не дал targetable окна для Computer Use. Визуальное меню, звук и геймплей
не подтверждены; startup logs и swapchain сами по себе их не доказывают.
