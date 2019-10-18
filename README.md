## Opis problemu
#### Krótki opis
Proces realizujący program obsługi Pyrkonu może znajdować się w trzech stanach (poniżej tylko krótki opis):

- Przed Pyrkonem (BEFORE_PYRKON) - Proces „czeka w kolejce, żeby uczestniczyć w aktywnościach na Pyrkonie”.
- Na Pyrkonie (ON_PYRKON) - Proces „jest już na Pyrkonie, może uczestniczyć w warsztatach”.
- Po Pyrkonie (AFTER_PYRKON) - Proces „wyszedł z Pyrkonu i czeka na inne procesy żeby rozpocząć zabawę na nowo”.

Procesy wymieniają między sobą nastepujące rodzaje komunikatów:
- WANT_TO_ENTER - Proces informuje, że chcę dostać się na Pyrkon (komunikat z argumentem 0) lub na któryś z „n” warsztatów (liczby od 1 do n).
- ALRIGHT_TO_ENTER - Proces pozwala innemu procesowi na wejście na Pyrkon lub któryś z warsztatów.
- EXIT - Proces informuje inne procesy, że wyszedł już z Pyrkonu.
#### Długi Opis
Proces chcąc dostać się na Pyrkon informuje o tym wszystkie pozostałe procesy, stając w ten sposób w kolejce. Otrzymujący zapytanie o zgodę na wejscie na Pyrkon proces moze wyrazić zgodę (jeśli sam się własnie nie ubiega i nie uczestniczy lub znajduje sie w kolejce za pytającym procesem) lub póki co nie odpowiadać. Kiedy proces uzyska odpowiednią liczbę zgód, gwarantującyh istnienie dla niego miejsca na Pyrkonie, wchodzi (warunek: liczba otrzymanych zgód >= liczba procesów - maksymalna liczba procesów na Pyrkonie). Kiedy proces wychodzi z Pyrkonu zwalnia miejsce, odpowiadając na wcześniejsze zapytania, na które nie udzielił jeszcze odpowiedzi. Sytacja jest analogiczna dla warsztatów. Wychodząc z Pyrkonu proces informuje o tym wszystkie pozostałe. Kiedy proces będzie po Pyrkonie i otrzyma łacznie n-1 (gdzie n to liczba procesów) informacji o wyjściu procesów z Pyrkonu, rozpoczyna się nowy festiwal.
## Złożoność czasowa i komunikacyjna
Złozoność czasowa jest funkcją kosztu wykonania algorytmu rozproszonego, wyrażona przez liczbę kroków algorytmu do jego zakończenia przy założeniu, że:

- czas wykonywania każdego kroku (operacji) jest stały
- kroki wykonywane są synchronicznie
- czas transmisji wiadomości jest stały

Przyjmuje się też na ogół, że:

- czas przetwarzania lokalnego (wykonania kazdego kroku) jest pomijalny (zerowy)
- czas transmisji jest jednostkowy
Złozoność komunikacyjna pakietowa jest funkcją kosztu wykonania algorytmu wyrażona przez liczbę pakietów (wiadomości) przesyłanych w trakcie wykonywania algorytmu do jego zakończenia.

Algorytm można podzielić na trzy części odpowiadajęce kolejnym zadaniom realizowanym przez zaproponowany algorytm:

- wejście na Pyrkon - od podjęcia decyzji o wejściu na Pyrkon do faktycznego wejścia:
    - Proces wysyła do wszystkich informacje, ze chce sie dostac na Pyrkon. (czas: 1; komunikatów: n-1)
    - Otrzymujący informacje proces moze wyrazić zgodę lub nie odpowiadać jeszcze, lecz w końcu to zrobi. (czas: 1; komunikatów: n-1)
    - Po otrzymaniu odpowiedniej liczby zgód (liczba zgód gwarantująca, że jego wejście nie przekroczy maksymalnej ilości uczestników na Pyrkonie) proces wchodzi. Pozostałe odpowiedzi otrzymuje, nie wpływają one na przetwarzanie.
- Szacowana złożoność to:
    - czasowa: 2
    - komunikacyjna: 2*(n-1)
- wejście na Warsztat - analogiczne jak dla wejścia na Pyrkon
- nowy Pyrkon
    - Proces wychodzący z Pyrkonu informuje o tym wszystkie pozostałe procesy. Nowy Pyrkon rozpocznie się w momencie, kiedy liczba zebranych informacji o wyjściu z Pyrkonu będzie równa liczbie procesów.
Szacowana złożoność to:
    - czasowa: 1
    - komunikacyjna: n-1
