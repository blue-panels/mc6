---
date: wrzesień 2026
---

# NAZWA <!-- help:skip -->

mcedit6 - Wbudowany edytor plików.

# UŻYTKOWANIE <!-- help:skip -->

**mcedit6**
[-bcCdfhstVx?] plik

# Wbudowany edytor plików <a id="internal-file-editor"></a>

Wbudowany edytor plików to pełnoekranowy edytor ze wszystkimi zwykłymi
możliwościami. Plik większy niż
*editor_filesize_threshold*
(domyślnie 64 MB) otwiera się po pytaniu. Pliki binarne można edytować.
Edytor jest wywoływany klawiszem
**F4**,
o ile opcja
*use_internal_edit*
jest ustawiona w pliku startowym.

Obsługiwane możliwości to: kopiowanie, przenoszenie, kasowanie, wycinanie i
wklejanie bloków; cofanie klawisz po klawiszu; menu rozwijalne; wstawianie
plików; makra; szukanie i zastępowanie wyrażeniami regularnymi; zaznaczanie
tekstu strzałkami z Shiftem (jeśli terminal je rozróżnia); przełączanie
wstawiania i zastępowania; zawijanie wierszy; automatyczne wcięcia;
ustawialna szerokość tabulacji; podświetlanie składni dla różnych typów
plików; oraz przepuszczanie bloku tekstu przez polecenie powłoki, takie jak
indent czy ispell.

Rozdziały:
: [Opcje edytora](#editor-options)

Edytor jest bardzo prosty w użyciu i nie wymaga przygotowania. Aby zobaczyć,
co robią klawisze, wystarczy obejrzeć odpowiednie menu rozwijalne. Poza tym
strzałki z Shiftem zaznaczają tekst.
**Ctrl-Ins**
kopiuje do pliku wymiany
**~/.local/share/mc6/mcedit/mcedit.clip**,
**Shift-Ins**
wkleja z niego,
**Shift-Del**
wycina do niego, a
**Ctrl-Del**
kasuje zaznaczony tekst. Zaznaczanie myszą też działa, a można je przesłonić
zwykłym zaznaczaniem terminala, trzymając Shift podczas przeciągania.

Aby zdefiniować makro, naciśnij
**Ctrl-R**,
a potem te klawisze, które mają zostać wykonane. Naciśnij ponownie
**Ctrl-R**,
kiedy skończysz, i przypisz makro do klawisza, naciskając go. Makro wykonuje
się tym klawiszem, a także skrótem
**Ctrl-A**
i przypisanym klawiszem. Makra są przechowywane w sekcji
**[editor]**
pliku
**~/.local/share/mc6/macros**,
a kasuje się je, usuwając ich wiersz z tego pliku.

Zestaw znaków wyświetlanego tekstu zmienia Alt-e (M-e). Przekodowanie idzie z
wybranej strony kodowej na systemową. Aby je wyłączyć, wybierz
"\<No translation>" w oknie wyboru zestawu znaków.

Przycisk
**Filtr**
w oknie szukania
(**F7**)
ukrywa wszystkie wiersze, w których nie ma szukanego tekstu, z tymi samymi
ustawieniami rodzaju szukania, wielkości liter i całych słów co szukanie.
Numery wierszy pozostają oryginalne, a kolumna stanu zaznacza każdy ukryty
fragment. Zbiór ukrytych wierszy ustala się w chwili naciśnięcia przycisku:
edycja nie sprawdza warunku ponownie, więc widoczny wiersz, który zostanie
podzielony albo złączony, pozostaje widoczny, a wiersze napisane później też
pozostają widoczne, nawet jeśli nie pasują do warunku.
**M-s**
zdejmuje filtr; naciśnięty ponownie zakłada ostatnie szukanie jako filtr.
Pozycja "Rozwiń wszystko" w menu Polecenia również go zdejmuje.

# Opcje edytora <a id="editor-options"></a>

Ustawienia
[wbudowanego edytora](#internal-file-editor).
Otwiera je menu
**Opcje**
samego edytora oraz pozycja
**Opcje edytora**
menu Opcje menedżera plików.

*Tryb zawijania.*
Wyłączony, bieżące formatowanie akapitu albo zawijanie maszynowe, które łamie
wiersz na zadanej długości w trakcie pisania.

*Udawanie połówek tabulacji.*
Między tekstem a lewym marginesem ruch i wcięcie idą o pół tabulacji i
wypełniane są spacjami; w pozostałych miejscach tabulacja jest zwykła.

*Backspace przez tabulacje.*
Jedno naciśnięcie Backspace usuwa całe wcięcie do lewego marginesu, gdy między
kursorem a marginesem nie ma tekstu.

*Wypełnianie tabulacji spacjami.*
Zamiast znaku tabulacji wstawiane są spacje do następnej pozycji tabulacji.

*Szerokość tabulacji.*
Szerokość, którą zajmuje znak tabulacji. Domyślnie 8.

*Enter robi wcięcie.*
Nowy wiersz zaczyna się z wcięciem wiersza powyżej.

*Potwierdzanie zapisu.*
Przed zapisaniem pliku program pyta.

*Zapamiętywanie pozycji w pliku.*
Plik otwiera się tam, gdzie go ostatnio zostawiono.

*Pokazywanie spacji na końcu wiersza.*
Spacje na końcu wiersza są oznaczane.

*Pokazywanie tabulacji.*
Znaki tabulacji są oznaczane.

*Pokazywanie znaków sterujących.*
Znaki sterujące tekstu są wypisywane, a nie ukrywane.

*Podświetlanie składni.*
Tekst jest kolorowany regułami składni dla jego typu pliku.

*Kursor za wstawionym blokiem.*
Po wstawieniu bloku kursor zostaje na jego końcu, a nie na początku.

*Trwałe zaznaczenie.*
Zaznaczenie pozostaje przy ruchu kursora, zamiast znikać.

*Kursor za końcem wiersza.*
Kursor może stać za ostatnim znakiem wiersza.

*Cofanie grupowe.*
Jedno cofnięcie przywraca serię zmian tego samego rodzaju, a nie jedno
naciśnięcie klawisza.

*Długość wiersza przy zawijaniu.*
Kolumna, na której tryby zawijania łamią wiersz. Domyślnie 72.

# Zapisz jako <a id="save-file-as"></a>

Nazwa, pod którą zapisać plik, i końce wierszy, z którymi go zapisać: takie,
jakie są w pliku, Unix (LF), Windows i DOS (CR LF) albo Macintosh (CR).

# Tryb zapisu <a id="edit-save-mode"></a>

Jak zapisywany jest plik:

**Szybki zapis**
: Pisze od razu na pliku. Szybko, a awaria w połowie zostawia plik zapisany do
połowy.

**Bezpieczny zapis**
: Najpierw pisze plik tymczasowy i przemianowuje go na pierwotny, gdy jest
cały, więc awaria nie narusza oryginału.

**Kopie zapasowe z rozszerzeniem**
: Bezpieczny zapis, a oryginał zostaje pod swoją nazwą z dodanym rozszerzeniem
z linii wejściowej, domyślnie "~".

**Sprawdzanie końca wiersza POSIX**
: Pyta o brakujący koniec wiersza na końcu pliku przed zapisem.

# Przeglądarka makr <a id="macro-explorer"></a>

Nagrane makra, z klawiszem, na który każde odpowiada, i tym, co robi. Przyciski
to

**Uruchom**
: Odtwarza makro, na którym stoi kursor.

**Usuń**
: Usuwa je po pytaniu.

**Edytuj plik**
: Otwiera plik, w którym mieszkają makra.

# Otwarte pliki <a id="open-files"></a>

Pliki otwarte w edytorze, po jednym w wierszu. Enter przechodzi do pliku, na
którym stoi kursor, Esc zostawia pokazywany.

# Informacje o wtyczkach <a id="plugin-info"></a>

Wtyczki, które edytor wczytał: nazwa, czy jest włączona, co daje i co robi. To
lista do oglądania; wtyczkę wyłącza się i jej ustawienia otwiera w oknie
[Zarządzanie wtyczkami](mcommander.md#manage-plugins)
menedżera plików.

# ZOBACZ TAKŻE <a id="see-also"></a>

mcommander(1), mview(1).
