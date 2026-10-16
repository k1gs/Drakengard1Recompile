# Europe input: первичная приёмка

Дата наблюдения: 2026-10-16. Вход предоставлен пользователем в game/.
Оригинальный ISO не менялся; извлечение выполнено в отдельный ignored work/extracted-001/.
SHA-256 ISO повторно проверен после извлечения: совпал с исходным.
Идентификаторы ниже относятся только к этой копии; внешний эталон dump не сверялся.

## Fingerprints

| Объект | Размер, байт | SHA-256 |
| --- | ---: | --- |
| ISO | 4 445 044 736 | `73d3a3e7261ecfb134efbcf84c1b47278817e927dc62b5982f9bf95802be4706` |
| SLES_523.22 | 4 873 284 | `065a94591d295bb936a8dca92e722c3bd72cd05848c7274b26d572b0f782e918` |
| SYSTEM.CNF | 56 | `03aa62009ea428ce25dba4aac504bee436cb494905b03f56635c8ba16c333675` |

SYSTEM.CNF указывает BOOT2 `cdrom0:\SLES_523.22;1`, VER `1.00`, VMODE `PAL`.
Это наблюдаемая конфигурация загрузки, а не независимая проверка revision/authenticity.

## ELF

ELF32 little-endian, executable MIPS; entry `0x00120008` находится в file-backed RX segment.

| PT_LOAD | File offset | Guest VA | File size | Memory size | Flags |
| --- | --- | --- | ---: | ---: | --- |
| 0 | 0x00001000 | 0x00120000 | 2 251 288 | 2 251 288 | RX |
| 1 | 0x00226A80 | 0x00345A80 | 2 276 453 | 3 618 900 | RW |

Второй segment требует zero-fill tail 1 342 447 байт; это contract будущего loader.
Всего 1 763 section headers. Обычные SHT_SYMTAB/SHT_DYNSYM отсутствуют, однако
1 346 `.gnu.linkonce.t.*` секций сохраняют именованные code boundaries. Их нужно
валидировать по disassembly; они не описывают все функции основной `.text`.
Обнаружены 53 `.DVP.overlay.*` секции. Их наличие не доказывает динамические EE overlays;
содержимое и связь с VU/DVP требуют отдельного анализа.

## Состав диска

17 файлов, суммарно 4 444 438 201 байт. Имена позволяют поставить гипотезы о назначении,
но формат контейнеров и наличие конкретных языков ещё не проверялись.

| Группа | Наблюдаемые файлы |
| --- | --- |
| Boot | SYSTEM.CNF, SLES_523.22 |
| IOP reset image | MODULES/IOPRP280.IMG |
| IRX | CDVDSTM, EZSOUND, LIBSD, MCMAN, MCSERV, PADMAN, SDRDRV, SIO2MAN |
| Крупные BIN | IMAGE, FACEEU, SNDIMGEU, VOICEEU, MOVIE0EU, MOVIE1EU |

Не заменять IOP/SIF/audio/CD services заглушками на основании одних имён модулей.
Следующий шаг — baseline и карта реальных вызовов, затем выбор runtime paths.

## Воспроизведение локальной приёмки

Python 3.10.0; `bsdtar 3.8.8`, `libarchive 3.8.8` (системный Windows tar).
Сначала выполнена image fingerprint command из docs/INTAKE.md, затем `tar -tf` для
проверки имён: наблюдались только относительные пути без `..` и absolute entries.

```powershell
# Только после подтверждения готовности входа; каталог назначения должен быть новым.
New-Item -ItemType Directory -Path work/extracted-001
tar -xf "game/Drakengard (Europe) (En,Fr,De,Es,It).iso" -C work/extracted-001
python scripts/intake.py tree --input-complete --input work/extracted-001 --output work/intake-tree-001.json
```

Локальные evidence files: `work/intake-image-001.json`, `work/intake-tree-001.json`,
`work/elf-profile-001.json`, `work/profile-elf-001.py`. Последние два содержат подробности
section headers и локальный одноразовый profiler, не включённые в публичный Git.
Извлечение и metadata inspection не запускают guest code и не подтверждают playable port.
