# main <!-- help:notitle -->

```
 ┌─┬─┐                                  - -        _
 │ │ │─┌.┌┐┌┬┐┌┬┐┌┐┌┐┌┤┌┐┌.             .'')}_____/
 │   │ └.└┘      └└  └└└─┴               ~_/      )
 ┴   ┴  {{MC_VERSION:32}}                (_(_/-(_/
```
 based on GNU Midnight Commander

Główny ekran pomocy programu **M-Commander**.

Aby dowiedzieć się, jak używać interaktywnej pomocy, należy nacisnąć klawisz [Enter](#how-to-use-help). Można też przejść bezpośrednio do [spisu treści](#contents).

Program GNU Midnight Commander został napisany przez jego [autorów](#authors).

Program M-Commander jest dostarczany BEZ JAKIEJKOLWIEK [GWARANCJI](gnu-gpl-v3.0.md#warranty). Niniejszy program jest wolnym oprogramowaniem; można go rozprowadzać dalej na warunkach [GNU General Public License](gnu-gpl-v3.0.md#license).

# Okna zapytań <a id="querybox"></a>

W oknach dialogowych zapytań można używać klawiszy strzałek lub pierwszych liter, aby wybrać element albo kliknąć na przycisku.

# Jak używać pomocy <a id="how-to-use-help"></a>

Do obsługi przeglądarki można używać klawiszy kursora lub myszy. Naciśnięcie **strzałki w dół** przeniesie do następnego elementu lub przewinie w dół. Naciśnięcie **strzałki w górę** przeniesie do poprzedniego elementu lub przewinie w górę, Naciśnięcie **strzałki w prawo** podąży za zaznaczonym odnośnikiem. Naciśnięcie **strzałki w lewo** powróci do poprzednio odwiedzonego węzła.

Jeśli terminal nie obsługuje klawiszy kursora, można używać **spacji** do przewijania do przodu i klawisz **B**, aby przewijać do tyłu. Można używać klawisza **Tab**, aby przechodzić do następnego elementu i klawisza **Enter**, aby podążyć za zaznaczonym odnośnikiem. Klawisz **L** może być używany do przechodzenia do poprzednio odwiedzonego węzła. Naciśnięcie klawisza **Esc** zakończy przeglądarkę pomocy.

Lewy przycisk myszy podąży za odnośnikiem lub przewinie ekran. Prawy przycisk myszy może być używany, aby przechodzić do poprzednio odwiedzonego węzła.

Pełna lista klawiszy przeglądarki pomocy:

[Ogólne klawisze ruchu](#general-movement-keys) są akceptowane.

**Tab**           Następny element.
**M-Tab**         Poprzedni element.
**Dół**           Następny element lub przewijanie o wiersz w dół.
**Góra**          Poprzedni element lub przewijanie o wiersz w górę.
**Prawo**, **Enter**  Podążanie za zaznaczonym odnośnikiem.
**Lewo**, **l**       Ostatnio odwiedzony węzeł.
**F1**            Pomoc dla przeglądarki pomocy.
**N**             Następny węzeł.
**P**             Poprzedni węzeł.
**C**             Przejście do Spisu treści.
**F10**, **Esc**      Zakończenie działanie przeglądarki pomocy.

Local variables:
fill-column: 58
end:

# Przypisania klawiszy <!-- help:notitle --><a id="key-bindings"></a>

**Przypisania klawiszy**

Przeglądanie i zmiana skrótów klawiszowych dla działań programu.

**Klawisze**

**Enter**
: Zmienia skrót: naciśnij klawisz, który chcesz przypisać.

**F5**
: Dodaje kolejny skrót dla tego działania.

**F8, Del**
: Usuwa skrót.

**Zapisz**
: Zapisuje zmiany w pliku
*~/.config/mc6/keymap.ini*.

**Edytuj plik klawiszy**
: Otwiera
*keymap.ini*
w edytorze.

**Edytuj plik terminala**
: Otwiera definicje klawiszy terminala.

Działania oznaczone \* różnią się od domyślnych.

# Podsłuch klawiszy <!-- help:notitle --><a id="key-sniffer"></a>

**Podsłuch klawiszy**

Naciśnij Przechwyć, a potem dowolny klawisz. Pokazuje:

```
Skrót       Nazwa symboliczna (na przykład Ctrl-F5)
Działanie   Działanie przypisane w bieżącej mapie
Surowo      Sekwencja sterująca i bajty szesnastkowo
Kod         Wewnętrzny kod liczbowy
```

Przydatne przy szukaniu przyczyn kłopotów z klawiszami terminala.
