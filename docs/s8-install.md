# Установка iOS на Galaxy S8 (SM-G950F)

> ⚠️ **Риски.** Это экспериментальная сборка. Установка **стирает USERDATA** (все данные Android). Нужен разблокированный загрузчик (Knox срабатывает). Пишутся только два раздела: **BOOT (`sda7`)** и **USERDATA (`sda24`)**. EFS, recovery и загрузчики не трогаются, но **сделайте полный бэкап** до начала.
>
> Только **SM-G950F** (Exynos 8895, `dreamlte`). Snapdragon-версии (G950U/W/0) и S8+ (G955F) не поддерживаются: у S8+ другой экран и раздел может иметь другой номер.

## 0. Что нужно

- Galaxy S8 SM-G950F, OEM unlock, установленный **TWRP**.
- ПК: Linux или Windows 10/11 с **WSL Ubuntu**. Команды ниже для WSL/Linux; скрипты `.ps1` — для PowerShell на Windows.
- `adb` (platform-tools), `python3`, `gh` (GitHub CLI).
- В Linux/WSL: `gcc-aarch64-linux-gnu`, `hfsprogs` (`mkfs.hfsplus`, `fsck.hfsplus`), `rsync`, [`apfs-fuse`](https://github.com/sgan81/apfs-fuse) (собрать из исходников), модуль ядра `hfsplus` (в WSL его нужно собрать для ядра WSL и загрузить через `insmod`), [`ipsw`](https://github.com/blacktop/ipsw).
- ~40 ГБ свободного места.

## 1. Бэкап

Загрузитесь в TWRP, в «Backup» отметьте Boot, EFS, System, Data и сохраните на SD-карту или ПК. Дополнительно сырые копии:

```bash
adb shell "dd if=/dev/block/sda7 of=/sdcard/BOOT.img"
adb pull /sdcard/BOOT.img backup/BOOT.img
```

`backup/BOOT.img` (стоковый BOOT) нужен дальше: из него берутся заголовок и DTBH.

## 2. Ядро XNU

Ядро собирается в GitHub Actions на macOS (бесплатно для публичных репозиториев).

1. Сделайте fork этого репозитория.
2. Actions → **xnu** → *Run workflow* (`machine_config` = `A5XELTE`, по умолчанию). Сборка также запускается сама при изменении `xnu/patches/**`.
3. Скачайте артефакт:

```bash
gh run list -R YOU/REPO --workflow xnu --limit 1
gh run download RUN_ID -R YOU/REPO -n xnu-kernel -D xnu/out
```

Получится `xnu/out/kernel.development.a5xelte`. Конфиг называется A5XELTE, S8-часть добавляет патч `0124-dreamlte-s8.patch`.

## 3. Корень iOS

1. Скачайте сами IPSW **iPhone X (iPhone10,3 / iPhone10,6) iOS 16.7.16 (20H392)** с серверов Apple (ссылки есть на ipsw.me). Здесь он не распространяется.
2. Распакуйте IPSW (это zip). Имена образов OS, SystemOS и AppOS смотрите в `BuildManifest.plist`.
3. Device Tree iPhone X для MobileGestalt:

```bash
ipsw img4 im4p extract Firmware/all_flash/DeviceTree.d22ap.im4p -o research/ipsw-x/d22ap.adt
```

4. Соберите образ (16 ГБ, HFS+) от root:

```bash
sudo tools/mkiosroot.sh OS.dmg SystemOS.dmg AppOS.dmg ios16-root.img 16G
sudo tools/dreamlte/finish-iosroot.sh ios16-root.img
```

`finish-iosroot.sh` создаёт каталоги логов, применяет `tools/sweet-root-tweaks.py` (пропуск Setup, состояние активации, отключение вочдогов и т.п.) и проверяет ФС.

## 4. Запись корня в USERDATA

Телефон в TWRP. Образ пишется частями по 32 МиБ, каждая проверяется по SHA-256, после обрыва USB можно просто перезапустить.

```bash
python3 tools/dreamlte/chunk-manifest.py ios16-root.img ios16-root.json
```

В PowerShell:

```powershell
tools\dreamlte\flash-chunks.ps1 -Image \\wsl.localhost\Ubuntu\root\ios16-root.img -Manifest ios16-root.json -Partition USERDATA
```

Последние 4 МиБ USERDATA ядро использует как кольцо лога, образ 16 ГБ их не задевает.

## 5. BOOT-образ

```bash
bash tools/dreamlte/make-boot.sh xnu/out/kernel.development.a5xelte backup/BOOT.img s8-ios-boot.img \
  debug=0xc serial=3 cpus=4 keepsyms=1 -enable_kprintf_spam -noprogress rd=ufsroot0 ufsroot=USERDATA \
  unentitled_ios_sim_launch=1 ignition_level=1 wdt_test=no_user_checkin ufsrw=1 rootrw=1 \
  launchd_unsecure_cache=1 wdt=-1 a5touchprobe=0 a5fb_keep_on=1 a5fb_noconsole=1 sweetfb_native=1
```

Скрипт строит ADT (`tools/mkadt_dreamlte.py`), payload (`tools/mkpayload.py`), собирает загрузчик `shim/dreamlte` и упаковывает BOOT со стоковым заголовком.

Запись (телефон в TWRP):

```bash
adb push s8-ios-boot.img /tmp/boot.img
adb shell "dd if=/tmp/boot.img of=/dev/block/sda7 bs=1048576 conv=fsync; sync"
adb reboot
```

Проверьте, что `sda7` — это BOOT: `adb shell ls -l /dev/block/by-name/BOOT` (путь может отличаться).

## 6. Загрузка

- Сначала идут строки загрузчика, затем логотип Apple. До экрана блокировки **около 15 минут** (рисует CPU).
- `cpus=1` стабильнее, `cpus=4` быстрее на старте, но пока может подвисать (см. README).

### Вернуться в TWRP

Зажмите **Питание + Громкость вниз** (перезагрузка), сразу после гашения экрана — **Питание + Bixby + Громкость вверх**.

### Логи

В TWRP:

```powershell
tools\dreamlte\pull-sweetlog.ps1 boot1
```

Будут сохранены `logs/sweetlog-boot1.txt` (консоль ядра с диска) и `logs/pmsg-boot1.txt` (загрузчик и раннее ядро). Приложите их к issue.

## Вернуть Android

Восстановите BOOT и Data из бэкапа TWRP (или `dd` сохранённого `BOOT.img` в `sda7`), затем «Format Data» в TWRP.

## Параметры загрузки

| Параметр | Зачем |
| --- | --- |
| `cpus=N` | Сколько ядер A53 запускать (1–4) |
| `rd=ufsroot0 ufsroot=USERDATA` | Корень — HFS+ в разделе USERDATA |
| `ufsrw=1 rootrw=1` | Корень на запись |
| `sweetfb_native=1` | Показывать iOS панель 1440×2960 (1125×2436 пока зависает) |
| `a5fb_noconsole=1` | Не рисовать консоль ядра поверх iOS |
| `a5touchprobe=0` | Отключить пробу тача от Galaxy A5 (на S8 вызывает abort) |
| `wdt=-1 wdt_test=no_user_checkin` | Без сторожевых таймеров (система медленная) |
| `ignition_level=1 launchd_unsecure_cache=1` | Загрузка shared cache из cryptex без проверки подписи |
