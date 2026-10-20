# iOS on the Samsung Galaxy S8

**iOS 16.7 на Samsung Galaxy S8 (SM-G950F, Exynos 8895, `dreamlte`).**

Порт открытого ядра Apple XNU (xnu-8796, поколение iOS 16) на Galaxy S8. Поверх него работает **немодифицированный userland iOS 16.7.16 (20H392) от iPhone X**: launchd, системные демоны, backboardd и SpringBoard. Телефон доходит до **экрана блокировки iOS**.

> ⚠️ Экспериментальный проект, а не рабочая прошивка. Он очень медленный, многое не работает (см. [Известные баги](#известные-баги)). Установка **стирает раздел USERDATA** (все данные Android). Только для Exynos-версии **SM-G950F**.

English summary: [below](#english).

## Что работает

| Область | Состояние |
| --- | --- |
| Загрузка | Стоковый Samsung S-Boot → наш загрузчик в разделе BOOT → XNU. Recovery (TWRP), EFS и загрузчики не трогаются |
| CPU | 4 ядра Cortex-A53 (MPIDR 0x100–0x103), запуск через PMU + PSCI. Страницы 4 KiB, GIC-400, таймер 26 МГц |
| Память | 512 MiB из верхнего банка `0x880000000` |
| Хранилище | Свой драйвер Exynos UFS. Корень iOS — HFS+ в USERDATA (`sda24`), чтение/запись |
| iOS userland | Корневая ФС и shared cache iPhone X 16.7.16; launchd, демоны, backboardd, SpringBoard |
| Экран | Фреймбуфер S-Boot 1440×2960 (DECON в командном режиме). CoreAnimation рисует на CPU |
| GUI | Логотип Apple, экран блокировки iOS (подтверждено фото) |
| Кнопки | Питание, громкость +/−, Bixby (GPIO ALIVE) → HID-события (видно в логе) |
| Тач | **В работе.** Контроллер S6SY761 на `hsi2c_23`; проба в загрузчике готова, драйвер XNU — следующий шаг |
| Логи | Без фото экрана: лог загрузчика и ядра в ramoops (pmsg) + кольцо 4 MiB в конце USERDATA, читается из TWRP |

## Известные баги

- **Очень медленно и лагает.** Вся отрисовка на CPU, GPU нет. До экрана блокировки ~15 минут на одном ядре.
- **С 4 ядрами отрисовка подвисает** через несколько кадров. Потоки на дополнительных ядрах не получают таймерные пробуждения. Исправление (физический таймер CNTP, INTID 30) в тесте.
- **Тач пока не работает** в iOS (идёт разработка драйвера).
- Режим 1125×2436 (как у iPhone X) зависает на первом кадре; используется нативный 1440×2960 (`sweetfb_native=1`).
- Ядра Mongoose (большой кластер) не запускаются: нужно включить питание L2/кластера.
- Дата 31.12.1969: нет драйвера RTC.
- Нет батареи / зарядки (нет fuel gauge).
- Нет Wi-Fi, Bluetooth, сети, звука, камер, датчиков, GPS, SEP/Face ID.
- **Нет GPU.** Mali-G71 не может работать с Metal / драйверами AGX из iOS; ускорение графики нереально.
- Безопасность выключена: AMFI/sandbox разрешающие, заглушка AppleKeyStore. Не храните личные данные.

## Что можно добавить (roadmap)

- [ ] Драйвер тача S6SY761 в XNU (опрос по линии прерывания `gpa1-0`) → `SweetHIDTouch`
- [ ] Починить зависания на 4 ядрах, затем включить кластер Mongoose (ещё 4 ядра)
- [ ] Ускорить вывод: слои DECON без копирования, G2D для копирования, меньшее разрешение
- [ ] Режим 1125×2436 без зависания
- [ ] RTC (дата/время), заглушка/драйвер батареи (fuel gauge MAX77865)
- [ ] Больше ОЗУ (сейчас 512 MiB из 4 GiB)
- [ ] Вибромотор, яркость подсветки
- [ ] Wi-Fi (BCM4361) — большая работа

Помощь приветствуется: issues и pull request'ы.

## Установка

Подробная инструкция: **[docs/s8-install.md](docs/s8-install.md)**.

Коротко:
1. Разблокированный SM-G950F с TWRP, полный бэкап разделов.
2. Ядро XNU собирается в GitHub Actions (workflow `xnu`, `MACHINE_CONFIG=A5XELTE`).
3. Образ корня iOS собирается из IPSW iPhone X 16.7.16, который вы скачиваете сами, и записывается в USERDATA.
4. `tools/dreamlte/make-boot.sh` собирает BOOT-образ (загрузчик + XNU + Apple Device Tree), он записывается в `sda7` из TWRP.

## Как это загружается

```
S-Boot (стоковый загрузчик Samsung, EL1, MMU выключен)
  -> раздел BOOT: shim/dreamlte (наш загрузчик вместо Linux Image)
     -> разблокирует триггер DECON, пишет лог в ramoops, будит UFS
     -> копирует XNU + ADT из ramdisk в 0x880000000, boot_args
        -> XNU (xnu-8796, конфиг A5XELTE + патч 0124-dreamlte-s8)
           -> драйвер UFS -> HFS+ корень в USERDATA
              -> dyld + shared cache -> launchd -> backboardd + SpringBoard
```

## Структура

| Путь | Что это |
| --- | --- |
| `shim/dreamlte/` | Загрузчик для S-Boot: `loader.c`, пробы UFS / SMP / тача |
| `xnu/patches/` | Патчи XNU; S8-специфичный — `0124-dreamlte-s8.patch` |
| `tools/dreamlte/apply-dreamlte.py` | Генератор патча 0124 |
| `tools/mkadt_dreamlte.py` | Apple Device Tree для S8 |
| `tools/dreamlte/make-boot.sh` | Ядро → BOOT-образ |
| `tools/mkiosroot.sh`, `tools/dreamlte/finish-iosroot.sh` | Сборка корня iOS |
| `tools/dreamlte/flash-chunks.ps1` | Запись образа в USERDATA по частям с проверкой |
| `tools/dreamlte/pull-sweetlog.ps1` | Забрать логи загрузки из TWRP |
| `DREAMLTE_PROGRESS.md` | Журнал порта: факты о железе, каждая загрузка |

## Благодарности

- [orhunnx/ios-a5xelte](https://github.com/orhunnx/ios-a5xelte) — порт на Galaxy A5 2016 (4 KiB, Exynos, S-Boot), на котором основан этот проект
- [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet) — исходный порт iOS 16 на Redmi Note 10 Pro (README: [docs/upstream-README.md](docs/upstream-README.md))
- [MakrSas/LinuxS8](https://github.com/MakrSas/LinuxS8) — mainline Linux на Galaxy S8, справочник по тачу, кнопкам и UFS
- Mainline Linux (драйверы Exynos 8895, HSI2C, S6SY761), [uniLoader](https://github.com/ivoszbg/uniLoader)
- Открытые релизы Apple (xnu, IOHIDFamily, hfs), [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build), [ipsw](https://github.com/blacktop/ipsw), [apfs-fuse](https://github.com/sgan81/apfs-fuse)

iOS принадлежит Apple и здесь не распространяется: IPSW нужно скачать самостоятельно.

## Лицензия

Патчи XNU производны от XNU и подчиняются [APSL 2.0](https://opensource.apple.com/apsl/). Код, написанный по мотивам драйверов Linux (например `shim/dreamlte/touch_probe.c`), — GPL-2.0, см. заголовки SPDX. Исходные проекты ios-sweet и ios-a5xelte не указали общую лицензию, поэтому для остальных файлов отдельной лицензии пока нет.

## English

An experimental port of Apple's open-source XNU kernel (xnu-8796) to the Samsung Galaxy S8 (SM-G950F, Exynos 8895), running the unmodified iPhone X iOS 16.7.16 userland. It boots from the stock Samsung bootloader through our loader in the BOOT partition and reaches the **iOS lock screen**. It has 4 Cortex-A53 cores, its own UFS driver with an HFS+ root in USERDATA, and hardware buttons. Rendering is CPU-only and very slow; touch is in progress. See the tables above, [docs/s8-install.md](docs/s8-install.md) for installation, and [DREAMLTE_PROGRESS.md](DREAMLTE_PROGRESS.md) for the bring-up log. Based on [orhunnx/ios-a5xelte](https://github.com/orhunnx/ios-a5xelte) and [dipa-studio/ios-sweet](https://github.com/dipa-studio/ios-sweet).
