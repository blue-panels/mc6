# main <!-- help:notitle -->

```
 ┌─┬─┐                                  - -        _
 │ │ │─┌.┌┐┌┬┐┌┬┐┌┐┌┐┌┤┌┐┌.             .'')}_____/
 │   │ └.└┘      └└  └└└─┴               ~_/      )
 ┴   ┴  {{MC_VERSION:32}}                (_(_/-(_/
```
 based on GNU Midnight Commander

Questa è la principale schermata della guida interattiva del **M-Commander**. 

Per saperne di più su come usare la guida interattiva, premere semplicemente [invio](#how-to-use-help). Se lo si desidera, è possibile  consultare direttamente il [sommario](#contents) della guida.

Il Midnight Commander è stato scritto dai suoi [autori](#authors).

Il M-Commander NON E' COPERTO DA ALCUNA [GARANZIA](gnu-gpl-v3.0.md#warranty). Questo è software libero, lo si può ridistribuire sotto  certe [condizioni](gnu-gpl-v3.0.md#license).

# Finestre di dialogo di richiesta dati <a id="querybox"></a>

Nelle finestre di dialogo di richiesta dati è possibile usare i tasti freccia o la prima lettera per selezionare una voce o cliccare con il mouse sul bottone.

# Come usare la guida interattiva <a id="how-to-use-help"></a>

Usare i tasti del cursore o il mouse per navigare nel visualizzatore della guida.

Premere freccia in giù per spostarsi alla voce successiva o per spostarsi in basso. Premere freccia in su per spostarsi alla voce precedente o per spostarsi in alto. Premere freccia a destra per seguire il collegamento corrente. Premere freccia a sinistra per tornare indietro nello storico dei nodi visitati.

Se il terminale non supporta i tasti del cursore si può usare la barra spaziatrice per spostarsi in avanti ed il tasto 'b' per tornare indietro. Usare il tasto TAB per spostarsi sulla prossima voce e premere INVIO per seguire il collegamento corrente. Usare il tasto 'l' per tornare indietro nello storico dei nodi visitati. Premere ESC per uscire dal visualizzatore della guida.

Il tasto sinistro del mouse segue il collegamento  o sfoglia le pagine. Il tasto destro del mouse torna indietro nello storico  dei nodi visitati.

Elenco completo dei tasti del visualizzatore della guida:

Sono accettati i [tasti generali di movimento](#general-movement-keys).

tab           Va alla voce successiva.
M-tab         Va alla voce precedente. 
giù           Va alla voce successiva o una riga in basso.
su            Va alla voce precedente o una riga in alto.
destra, invio Segue il collegamento corrente.
sinistra, l   Torna indietro nello storico dei nodi visitati.
F1            Mostra la guida per il visualizzatore della 
guida stessa.
n             Va al nodo successivo.
p             Va al nodo precedente.
c             Va al nodo del sommario.
F10, esc      Esce dal visualizzatore della guida.

Local variables:
fill-column: 58
end:

# Associazioni dei tasti <!-- help:notitle --><a id="key-bindings"></a>

**Associazioni dei tasti**

Vedere e cambiare le combinazioni di tasti delle azioni del programma.

**Tasti**

**Invio**
: Sostituisce la combinazione: premere il tasto da assegnare.

**F5**
: Aggiunge un'altra combinazione per questa azione.

**F8, Canc**
: Toglie la combinazione.

**Salva**
: Scrive le modifiche in
*~/.config/mc6/keymap.ini*.

**Modifica file dei tasti**
: Apre
*keymap.ini*
nell'editor.

**Modifica file del terminale**
: Apre le definizioni dei tasti del terminale.

Le azioni segnate con \* si scostano da quelle di serie.

# Analizzatore di tasti <!-- help:notitle --><a id="key-sniffer"></a>

**Analizzatore di tasti**

Premere Cattura e poi un tasto qualsiasi. Mostra:

```
Combinazione   Nome simbolico (per esempio Ctrl-F5)
Azione         Azione associata nella mappa attuale
Grezzo         Sequenza di escape e byte in esadecimale
Codice         Codice numerico interno
```

Utile per capire i problemi con i tasti del terminale.
