# CHANGES

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
