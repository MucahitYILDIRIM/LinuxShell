# CHANGES

## Kapsam incelemesi ve eksik testlerin tamamlanmasi (2026-09-24)

Mevcut test dosyasi (`tests/test_osProje.c`, 31 test) `osProje.c` icindeki tum
statik yardimci fonksiyonlari (`isPipe`, `tokenizeLine`, `parseRedirection`,
`parseBackground`, `singleProccessing`, `singleProccessingBg`, `bgHandlerControl`,
`inputProcessing`, `outputProcessing`, `execPipe`) kapsiyordu. Kapsam
incelemesinde `osProje.c` icindeki asagidaki fonksiyonlarin test edilmedigi
tespit edildi ve testleri eklendi (34 test, 0 hata):

- **`welcomeScreen()`** – karsilama banner'inin basildigi dogrulandi
  (`test_welcomeScreen_printsBanner`).
- **`PromptBas()`** – ciktinin hostname, `getcwd()` sonucu, ANSI renk kodlari
  (`GRN`/`BLU`/`RESET`) ve sondaki `>` karakterini icerdigi dogrulandi; `LOGNAME`
  ortam degiskeni tanimliysa kullanici adinin da ciktida yer aldigi kontrol edildi
  (`test_PromptBas_containsHostnameCwdAndColorCodes`).
- **`program_quit()`** – `exit(0)` cagirdigi icin ayri bir process'te (fork)
  calistirildi; bir torun surecin islemini bitirip dosyaya yazmasi beklenip,
  `program_quit`in bu torun sureci topladiktan SONRA cikis kodu 0 ile
  sonlandigi dogrulandi (`test_program_quit_waitsForChildrenThenExitsZero`).

### Karsilasilan ve duzeltilen test hatasi (davranis degisikligi degil)
`program_quit()` testinde ilk denemede, `make test | tail` gibi ciktinin bir
boruya yonlendirildigi durumlarda stdout'un tam tamponlu (fully buffered)
oldugu, `fork()` oncesi tampon bosaltilmadigi icin `program_quit()` icindeki
`exit(0)` cagrisinin cocuk surecte ana surecin henuz yazdirilmamis Unity test
ciktisini tekrar bastirdigi gozlemlendi (yinelenen "PASS" satiri). Diger
fork tabanli testlerdeki gibi `captureStart()` (once `fflush(stdout)` yapip
stdout'u gecici dosyaya yonlendiriyor) `fork()`tan ONCE cagrilarak duzeltildi;
bu, uretim kodunda (`osProje.c`) herhangi bir degisiklik gerektirmedi, sadece
test izolasyonuyla ilgiliydi. 15 ardisik calistirmada kararlilik dogrulandi.

### `main()` bilerek test disi birakildi
`main()` icindeki tum saf ayristirma mantigi zaten `tokenizeLine`,
`parseRedirection`, `parseBackground` fonksiyonlarina cikarilmis ve test
edilmis durumda. Geriye kalan `main()` govdesi stdin'den okuyan sonsuz bir
REPL dongusu ve `quit` disinda cikis yolu olmayan interaktif bir yapi; bunu
unit test etmek icin ya derlenmis binary'yi ayri bir surecte calistirip
stdin/stdout'unu kontrol eden bir entegrasyon testi (senkronizasyon ve
zamanlama riski yuksek, mevcut testlerin kapsadigi mantigi tekrar test eder)
ya da davranisi degistiren daha buyuk bir refactor gerekirdi. Kural 2 geregi
sadece gerekli oldugu kadar kucuk refactor yapilmasi istendiginden ve
`main()`in tum alt-mantigi zaten birim test kapsaminda oldugundan, `main()`
oldugu gibi (`#ifndef UNIT_TEST` ile testten izole) birakildi.

### Ortam
macOS (Darwin 25.6, Apple clang) uzerinde derlendi ve calistirildi: **34 test,
0 hata**; kararsizlik kontrolu icin 15 kez tekrar calistirildi, hepsi gecti.
Linux'ta calistirilmadi; kullanilan API'ler (POSIX `gethostname`, `getcwd`,
`waitpid`) Linux'ta da mevcut.

---

## Unit testler eklendi

### Kararlar
- **Framework: Unity (ThrowTheSwitch, v2.6.0).** C için en yaygın, bağımlılıksız ve
  depoya gömülebilen (vendored) test framework'ü. `tests/unity/` altında
  (`unity.c`, `unity.h`, `unity_internals.h`, MIT `LICENSE.txt`). Paket yöneticisi gerektirmez.
- **Çalıştırma:** `make test` (makefile'a `test` hedefi eklendi; varsayılan `program`
  hedefi değişmedi).
- **Test yöntemi:** `tests/test_osProje.c`, `osProje.c`'yi doğrudan `#include` ediyor;
  `UNIT_TEST` tanımlıyken `main` derlenmiyor. Böylece `static` değişkenler dahil her şeye
  erişiliyor ve ayrı bir header/kütüphane gerekmiyor.
- fork/exec yapan fonksiyonlar (`singleProccessing`, `inputProcessing`, `outputProcessing`,
  `execPipe`, `bgHandlerControl`) gerçek komutlarla (`echo`, `cat`, `tr`, `sort`, `sh`)
  test edildi; stdout geçici dosyaya `dup2` ile yönlendirilerek çıktılar doğrulandı.
  Her testten sonra `SIGCHLD` varsayılana döndürülüyor ve artakalan çocuklar toplanıyor.
- `.gitignore` eklendi (`osProje`, `tests/test_osProje` derleme çıktıları).

### Refactor (davranış değiştirmeden)
- `main` içindeki saf ayrıştırma mantığı aynen fonksiyonlara taşındı:
  - `tokenizeLine(line, tokens)` – strtok ile tokenlere ayırma (0 = boş satır).
  - `parseRedirection(command, args, &counter)` – `<`/`>` tespiti (0/1/2).
  - `parseBackground(args, command)` – `&` tespiti.
- `main` `#ifndef UNIT_TEST` ile sarıldı.
- Doğrulama: orijinal (HEAD) ve refactor edilmiş binary'ye aynı komut dizisi verilip
  çıktılar karşılaştırıldı — birebir aynı.

### Ortam
- macOS (Darwin 25.6, Apple clang) üzerinde derlendi ve çalıştırıldı:
  **31 test, 0 hata**; kararsızlık kontrolü için 20 kez tekrarlandı, hepsi geçti.
  Linux'ta çalıştırılmadı; kullanılan API'ler (POSIX `waitid`, `mkstemp`, `sigaction`) Linux'ta da mevcut.

### Tespit edilen ama düzeltilmeyen hatalar (davranış korunması için dokunulmadı)
1. **`main` – `;` ekleme sonrası tanımsız davranış:** Satır `;` ile bitmiyorsa
   `tokens[countLen] = ";"` sonlandırıcı `NULL`'ın üzerine yazıyor; `tokens[countLen+1]`
   ilklendirilmemiş kalıyor ve döngü onu okuyor. Önceki satırın kalıntısına göre
   fazladan komut çalışabilir/çökebilir. Düzeltme: `tokens[countLen+1] = NULL;`.
2. **`execPipe` yalnızca 2 komutu destekliyor:** Tek `pipe()` çağrısı yapılıyor;
   `a | b | c` yanlış fd'lerle çalışıyor.
3. **`execPipe` – exec başarısız olursa çocuk `return 1` yapıyor:** Çocuk süreç
   `main` döngüsüne geri dönüp ikinci bir shell olarak çalışmaya devam ediyor
   (`_exit` olmalı). Bu yüzden bu yol test edilmedi.
4. **`singleProccessing` – `background` 0/1 dışındaysa dönüş değeri yok** (tanımsız davranış).
5. **`singleProccessing` – "Command not Found"** tamponda kalıyor; çocuk `SIGTERM` ile
   öldüğü için mesaj hiçbir zaman ekrana basılmıyor.
6. **`parseRedirection` sonrası dosya adı eksikse** (`ls >`) `NULL` dosya adı
   `open`/`access`'e geçiyor (test ile belgelendi).
7. **`program_quit`** – döngü, meşgul beklemeyle (busy-wait) yalnızca *ilk* biten
   arka plan işini bekleyip çıkıyor; birden fazla iş varsa diğerleri beklenmiyor.
8. **`bgHandlerControl` sinyal handler'ında `printf`** kullanıyor (async-signal-safe değil).
