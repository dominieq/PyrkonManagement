# PyrkonProject

WERSJA SŁOWNA:

Ustalenie liczby biletów na pyrkon, liczby warsztatów oraz miejsc na poszczególne warsztaty.
W wysyłanej wiadomości znajduje się liczba biletów (b) oraz liczba biletów na warsztaty (tablica z w)

1. START - BILET WSTĘPU:

    a) Procesy procesy wysyłąją komunikaty z prośbą o dostęp do puli biletów oraz ewentualną zgodą.
    
        zegar, id
            
    b) Gdy proces uzyska zgodę, zmniejsza liczbę biletów i zapisuje nową wartość w komunikacie.
        
        zegar, id, bilety
            
    c) Przy odbieraniu brane jest minimum z lokalnej liczby biletów i liczby biletów w wiadomości.
        
        po uwzględnieniu zegara, z tej nowszej części
                    
    d) Jeżeli jest zero biletów, proces nie może przejść dalej.
             
        zwalnia kolejkę do sekcji krytycznej, wychodzi z kolejki i staje w niej jeszcze raz
        odpowiada pozostałym, że wyszedł z sekcji krytycznej i na zaległe wiadomości
            
    e) Gdy jakiś proces dostanie komunikat, w którym b = 0, wtedy wysyła do innych procesów wiadomość o braku biletów.
                    
        gdy b == 0, ale z nowszym zegarem
                    
    f) Procesy czekają na wiadomość o wyjściu procesu z pyrkonu
        
        gdy liczba dostępnych biletów == b oraz brak aktywnego żądania dostępu, 
        ostatni proces ogłasza nowy Pyrkon

2. PYRKON - BILET NA WARSZTAT:

    a) Proces losuje liczbę warsztatów ( < i) oraz numery warsztatów.
    
    b) Następnie zaczyna się ubiegać o dostęp na warsztat (podobnie jak o bilet wstępu):
    
        Procesy procesy wysyłąją komunikaty z prośbą o dostęp do puli biletów oraz ewentualną zgodą.
        Gdy proces uzyska zgodę, zmniejsza liczbę biletów i zapisuje nową wartość w komunikacie.
        Przy odbieraniu brane jest minimum z lokalnej liczby biletów i liczby biletów w wiadomości.
                    // po uwzględnieniu zegara, z tej najnowszej
        Jeżeli jest zero biletów, proces nie może przejść dalej.
                    // zwalnia kolejkę do sekcji krytycznej?
        Gdy jakiś proces dostanie komunikat, w którym w_i = 0, wtedy wysyła do innych procesów wiadomość o braku biletów.
        Procesy czekają na wiadomość o wyjściu procesu z warsztatu w_i
        
    c) Każdy warsztat jest rozróżnialny więc ubieganie się o warsztat i nie przeszkadza w ubieganiu się o warsztat i+1 itd..
    
    d) Gdy na warsztat nie miejsca proces powinien ubiegać się na inne warsztaty (jeśli mu jakieś zostały).
    
    e) Po wyjściu z warsztatu proces wysyła wiadomość, że zwolniło się miejsce na danym warsztacie.

3. PYRKON - WYJŚCIE, CZKANIE NA NASTĘPNY:

    a) Po wyjściu z pyrkonu proces wysyła komunikat o jego opuszczeniu
    
    b) Proces czeka na wszystkie inne procesy i kiedy wszystkie wyjdą rozpoczyna się nowy pyrkon



##################

1. BILET WSTĘPU

    a) Procesy wysyłają komunikat z prośbą o wejście na Pyrkon.

        {my_clock, my_id, my_request=0}

    b) Jeśli proces dostanie prośbę o zgodę na wejście na Pyrkon od innego procesu, którego zegar w momencie żądania jest wcześniejszy niż zegar w naszym żądaniu, to odsyłamy zgodę.
    
        {my_clock, my_id, your_request=0, answer="true"}

    W przeciwnym razie nie wysyłamy nic. Nasze żądanie wysłane wcześniej wystarczy za odpowiedź.
    Dodajemy informację, że ktoś jest za nami w kolejce.

        after_me++

    Każdy proces ma tabicę, na której zaznacza, czy wysyłał wiadomość ze zgodą do danego procesu.

        tab_my_answer[all] = {boolean : answer = yes/no}
        
        //yes - udzieliłem zgody
        //no - jeszcze nie udzieliłem zgody
    
    Każdy proces ma tablicę, na której zaznacza, czy dostał zgodę od danego procesu.

        tab_my_request[all] = {boolean : request = yes/no}

        //yes - otrzymałem zgodę
        //no - jeszcze nie otrzymałem zgody

    c) Na podstawie udzielonych przez proces zgód i próśb pochodząych od innych procesów wyliczamy widełki dla naszego miejsca w kolejce:

        dane: 

            b - liczba biletów = sekcji krytycznych na wejście na Pyrkon

            all - liczba wszystkich uczestników = liczba procesów łącznie

            before_me - liczba uczestników w kolejce przede mną = liczba procesów z mniejszym zegarem Lamporta niż mój (dla momentu deklaracji żądania)

            after_me - liczba uczestników w kolejce za mną = liczba procesów z mniejszym zegarem Lamporta niż mój (dla momentu deklaracji żądania) -> to oznacza, że na pewno wejdę przed nimi 

        analiza:
        
            "mogę wejść, jeśli szacuję, że przede mną jest mniej niż dostępnych"
            
            "all - after_me => maksymalnie przede mną"
    
            b - (all - after_me) > 0 = wchodzę
            else czekam
            
                /* nie mogę wejść, jeśli przede mną jest więcej niż dostępnych
                    before_me > b = czekam */
        
    d) Kiedy już wyjdziemy, odpowiadamy na wszystkie wcześniejsze żądania z tablicy udzielając zgody (tam, gdzie jej nie udzieliliśmy jeszcze, niezależnie, czy dostaliśmy żądanie).

        {my_clock, my_id, your_request=0, answer="true"}

    Powiadamiamy również wszystkie procesy, że wyszliśmy. Informacje o tym procesy przechowują w tablicy.

        finish[all] = {boolean : finish = yes/no}

        //yes - zakończył
        //no - w przeciwnym razie

        {my_clock, my_id, my_request=-1}

    e) Jeśli ktoś zapełni tablicę finish, to można zacząć nowy Pyrkon (musieli zakończyć już wszyscy)

2. BILET NA WARSZTAT

    Jest analogicznie jak w przypadku biletu na Pyrkon. #TODO /* do ustalenia, kiedy wybiera */ Na samym początku uczestnik wybiera, na które warsztaty chce się udać.

    Kiedy uczestnik dostanie się na Pyrkon i chce udać się na warsztat pyta się wszystkich o to, czy może wejść. Proces przebiega analogicznie do dostania się na Pyrkon.

    Jeśli uczestnik nie chce iść na dany warsztat również udziela zgodę.

    Korzystamy z tablicy finish (skoro ktoś wyszedł z Pyrkonu, to na pewno nie jest przed nami w kolejce).

    TODO /* do ustalenia, czy tak robimy */ Można przygotować wersję, że na raz ubiegamy się o miejsce na więcej niż jeden warsztat. Wchodzi wówczzs tam, gdzie szybciej uzyska wymagany próg. Przy czym należy ustalić, czy w memencie kiedy wchodzimy na jakiś warsztat zwalniamy miejsca w kolejkach na pozostałe i musimy stanąć w kolejce ponownie. Wymaga to częstsze aktualizowanie, ale wydaje się, że programistycznie nie będzie dużo trudniejsze.




b - biletów = sekcji krytycznych na wejście na Pyrkon
analogicznie "w" dla warsztatów (osobne)


sekcja_krytyczna:
    ilość miejsc
    ilość w środku

kolejka:


Każdy proces ma tabelę z sekcjami krytycznymi, na które nie udzielił odpowiedzi.


1) Proces chce dostać się do sekcji krytycznej, wysyła żądanie do innych.
    - zegar
    - id
2) Proces odpowiada na każde żądanie udzielając zgody lub wysyłając własne żądanie
    - zegar
    - id
    - zgoda lub żądania
    -- aktualizacja
    ** analiza
3) Proces otrzymuje odpowiedź (zgodę lub żądanie)
    -- aktualizacja
    ** analiza
4) wyjście (Odpowiedź na poprzednie żądania przez informację o wyjściu)


//tablica
-- aktualizacja
    zegar
    żądania



łącznie = liczba wszystkich procesów
przede mną | ja | za mną

przede_mną - liczba procesów z zegarem Lamporta mniejszym niż mój własny
ja - mój zegar z momentu żądania
za_mną - liczba procesów z zegarem większym niż mój własny

b - liczba biletów

** analiza {

    // mogę wejść, jeśli szacuję, że przede mną jest mniej niż dostępnych
    // łącznie - za_mną => maksymalnie przede mną
    b - (łącznie - za_mną) > 0 = wchodzę
    //else czekam /*// nie mogę wejść, jeśli przede mną jest więcej niż dostępnych
                                przede_mną > b = czekam*/

}

** wysyłanie zwrot {
    wysyłam do wszystkich, że wyszedłem
    jeśli z pyrkonu, to wysyłam też żądanie o nowy Pyrkon
}

** otrzymanie exit {
    zwiększa parametr za mną
}


dla wszystkich warsztatów wysyła jeden raz exit (albo po wyjściu z warsztatu, albo po wyjściu z Pyrkonu)




b = liczba ludzi na pyrkonie
i = liczba warsztatów
w_i = liczba miejsc na danym warsztacie

tab [i, za_mną_na_i]

