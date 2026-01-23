## Raport Projektowy
### Grzegorz Panna GĆ.03
### Temat 15: Ciastkarnia
#### Założenia projektowe
Projekt symuluje działanie ciastkarni, która produkuje P różnych produktów (P > 10) sprzedawanych w sklepie firmowym. Produkty trafiają na podajniki FIFO o pojemności Ki. Ciastkarnia działa w godzinach Tp-Tk, a sklep w Tp+30min-Tk. W sklepie może przebywać maksymalnie N klientów, a liczba czynnych kas (min. 1) zależy od liczby klientów (1 kasa na K = N/2 klientów). Klienci robią zakupy losowo, a brak produktu na podajniku oznacza jego pominięcie. Na sygnał inwentaryzacji (SIGUSR1) procesy podsumowują sprzedaż i produkcję po zamknięciu sklepu. Na sygnał ewakuacji (SIGUSR2) klienci przerywają zakupy i opuszczają sklep. Główne założenia obejmują:
- Synchronizację procesów za pomocą pamięci współdzielonej i semaforów.
- Obsługę sygnałów do zarządzania procesami (np. inwentaryzacja, ewakuacja).
- Kolejkowanie produktów w podajnikach FIFO.
- Logowanie zdarzeń w systemie do plików.

***Projekt był pisany na maszynie wirtualnej w środowisku
Ubuntu 24.04.3***

#### Ogólny opis kodu
Kod projektu składa się z kilku modułów:
1. **`ipc.c`**: Obsługuje pamięć współdzieloną i semafory.
2. **`logger.c`**: Zajmuje się logowaniem zdarzeń do plików.
3. **`podajnik.c`**: Implementuje push, pop FIFO dla produktów.
4. **`kierownik.c`**: Zarządza procesami i obsługuje sygnały.
5. **`kasjer.c`**: Obsługuje klientów i sprzedaż produktów.
6. **`klient.c`**: Symuluje klientów robiących zakupy.
7. **`piekarz.c`**: Produkuje produkty i dodaje je do podajników.

#### Zaimplementowane testy:
1. **`Test 1: Brak piekarza`**: W trakcie testu kierownik nie tworzy procesu piekarza.
Oczekiwanie: Żaden produkt nie jest wyprodukowany; Czego wynikiem jest 0 produktów sprzedanych;
2. **`Test 2: Pełny sklep`**: Przed otwarciem sklepu semafor klientów jest wymaksowany.
Oczekiwanie: Klienci czekają w kolejce i żaden nie wchodzi do sklepu;
3. **`Test 3: Spam klientów`**: Losowość w pojawieniu się klientów jest ignorowana, zjawiają się co iteracje pętli while.
Oczekiwanie: Program pomimo osiągnięciu limitów działa poprawnie, brak procesów zombie i porzuconych ipc;
3. **`Test 3: Brak klientów`**: W trakcie działania Kierownik nie tworzy żadnych klientów.
Oczekiwanie: "Czyste logi" Procesy kierownika, piekarza i kasjera nie wykonują żadnych nieoczekiwanych zadań, program kończy się poprawnie;


#### Co udało się zrobić
- Implementacja pamięci współdzielonej i semaforów do synchronizacji procesów.
- Obsługa sygnałów (np. SIGUSR1, SIGUSR2) do zarządzania procesami.
- Poprawne złapanie SIGINT.
- Kolejkowanie produktów w podajnikach FIFO z użyciem mutexów.
- Logowanie zdarzeń w systemie do plików `log` i `raport`.
- Obsługa klientów, kasjerów i piekarza w symulacji sklepu.

#### Problemy napotkane
- Synchronizacja procesów wymagała dokładnego zarządzania semaforami, aby uniknąć zakleszczeń.
- Obsługa sygnałów wymagała uwzględnienia wielu stanów procesów, aby uniknąć nieoczekiwanych zakończeń i porzucenia struktur IPC.
- Testowanie w środowisku wieloprocesowym było trudne ze względu na konieczność śledzenia wielu procesów jednocześnie.
- Program w czasie działania tworzył wiele procesów zombie, co zostało poprawione poprzez użycie waitpid.
#### Problemy z testami
- Testy w środowisku wieloprocesowym wymagały ręcznego monitorowania logów, co potrafiło być czasochłonne.
- W przypadku testu 4 Sygnały SIGUSR1 (Inwentaryzacja) a szczególnie SIGUSR2 (Ewakuacja) wywoływały problemy z poprawnym zwalnianiem IPC z powodu wielu sytuacji w których sygnał przerywał komunikacje.
---

### Linki do istotnych fragmentów kodu

#### a. Tworzenie i obsługa plików
- **`fopen()`**: [logger.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/logger.c#L18)
#### b. Tworzenie procesów
- **`fork()`**: [kierownik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kierownik.c#L82)
- **`exit()`**: [kasjer.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kasjer.c#L59C6-L59C12)
- **`wait()`**: [kierownik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kierownik.c#L371)
- **`waitpid()`**: [kierownik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kierownik.c#L152C19-L152C26)

#### c. Tworzenie i obsługa wątków
- **`pthread_create()`**: [kierownik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kierownik.c#L298)
- **`pthread_join()`**: [kierownik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kierownik.c#L394)
- **`pthread_mutex_lock()`**: [podajnik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/podajnik.c#L48)
- **`pthread_mutex_unlock()`**: [podajnik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/podajnik.c#L50)

#### d. Obsługa sygnałów
- **`signal()`**: [kasjer.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kasjer.c#L109), [kierownik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kierownik.c#L213C5-L213C36)
, [klient.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/klient.c#L58)
, [piekarz.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/piekarz.c#L69C7-L69C41)
, [kasjer.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kasjer.c#L109)
- **`kill()`**: [kierownik.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kierownik.c#L47)

#### e. Synchronizacja procesów (wątków)
- **`sem_open()`**: [ipc.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/ipc.c#L135C19-L135C27)
- **`sem_wait()`**: [ipc.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/ipc.c#L158)
- **`sem_post()`**: [ipc.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/ipc.c#L161)

#### f. Łącza nazwane i nienazwane
- **`mkfifo()`**: [kasjer.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kasjer.c#L64), [klient.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/klient.c#L65)
- **`write()`**: [kasjer.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kasjer.c#L210C13-L210C18), [klient.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/klient.c#L222)
- **`read()`**: [kasjer.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/kasjer.c#L179), [klient.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/klient.c#L253)

#### g. Segmenty pamięci dzielonej
- **`mmap()`**: [ipc.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/ipc.c#L122)
- **`shm_open()`**: [ipc.c](https://github.com/Gr0n/confectionery_simulator/blob/10697812ccc90b300963886e0105f7116f93ce0f/src/ipc.c#L111C18-L111C26)


