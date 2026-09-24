---
date: septiembre de 2026
---

<!-- help:topics "Índice de Contenidos:" -->
# NOMBRE <!-- help:skip -->

mcommander - entorno visual para sistemas tipo Unix.

# SINOPSIS <!-- help:skip -->

**mcommander**
[**-abcCdfFhPstuUVx**] [**-l**
*reg] [dir1 [dir2]]*
[**-e**
*[arch] ...]*
[**-v**
*arch]*

# DESCRIPCIÓN <a id="description"></a>

"M-Commander" (Comandante de Medianoche) es
un navegador de directorios/gestor de archivos para
sistemas operativos tipo Unix.


# OPCIONES <a id="options"></a>

*-a, --stickchars*
: Deshabilita el uso de caracteres gráficos para el dibujo de líneas.

*-b, --nocolor*
: Fuerza el uso de la pantalla de Blanco y Negro.

*-c, --color*
: Fuerza el uso del modo color. Véase la sección
[Colores](#colors)
para más información.

*--configure-options*
: Muestra opciones de configuración compiladas.

*-d, --nomouse*
: Deshabilita el soporte de ratón.

*-e [arch], --edit[=arch]*
: Iniciar el editor interno. Si se indica un archivo, editarlo. Véase la
página de manual de
**mcedit6(1)**.

*-f, --datadir*
: Muestra las rutas de búsqueda compiladas para archivos de M-Commander.

*-F, --datadir-info*
: Muestra información más extensa sobre las rutas de búsqueda compiladas
en M-Commander.

*-g, --oldmouse*
: Fuerza el uso de ratón en modo de seguimiento «normal». Se usa para
terminales compatibles con xterm (tmux/screen).

*-K arch, --keymap=arch*
: Carga desde un archivo la configuración de teclas para la línea de órdenes.

*-l reg, --ftplog=reg*
: Guarda el diálogo FTPfs con el servidor en el archivo.

*--nokeymap*
: No cargar asociaciones de teclas desde ningún archivo, utilizar las
teclas nativas del sistema.

*-P arch, --printwd=arch*
: Al salir del programa, M-Commander registrará el último
directorio de trabajo en el archivo indicado.  Esta opción no debe ser
usada directamente, sino desde un guión de shell adecuado, para dejar
como directorio activo el directorio que estaba en uso dentro de
M-Commander.  Consúltese en los archivos
**{{pkglibexecdir}}/mc6.sh**
(usuarios de bash y zsh),
**{{pkglibexecdir}}/mc6.csh**
(usuarios de tcsh) y
**{{pkglibexecdir}}/mc6.fish**
(usuarios de fish) la manera de definir
**mcommander**
como un alias para el correspondiente guión de shell.

*-s, --slow*
: Activa el modo para terminales lentos. En este modo el programa no
dibuja bordes con líneas de caracteres y desactiva el modo detallado.
Si no se rellena la sección [lines] el marco pseudo-gráfico estará
formado por espacios; en caso contrario el marco se contruye con
caracteres de texto según los parámetros siguientes:

**lefttop**
: esquina superior izquierda

**righttop**
: esquina superior derecha

**centertop**
: cruz superior central

**centerbottom**
: cruz inferior central

**leftbottom**
: esquina inferior izquierda

**rightbottom**
: esquina inferior derecha

**leftmiddle**
: cruz central izquierda

**rightmiddle**
: cruz central derecha

**centermiddle**
: cruz central

**horiz**
: línea horizontal por defecto

**vert**
: línea vertical por defecto

**thinhoriz**
: línea horizontal fina

**thinvert**
: línea vertical fina

*-S arg, --skin=arg*
: Permite elegir un «skin» o apariencia para mc.  La configuración de las
características de visualización (colores, líneas, etc.) se explica
detalladamente en la sección
[Skins](#skins).

*-t, --termcap*
: Usado solo si el código fue compilado con S-Lang y terminfo: hace que
M-Commander use el valor de la variable de entorno
**TERMCAP**
para obtener la información del terminal, en vez de la base de datos de
terminales del sistema.

*-u, --nosubshell*
: Deshabilita el uso de shell concurrente (solo tiene sentido si este
M-Commander fue construido con soporte de shell concurrente).

*-U, --subshell*
: Habilita el uso de shell concurrente (solo tiene sentido si este
M-Commander fue construido con soporte de subshell opcional).

*-v arch, --view=arch*
: Iniciar el visor interno para ver el archivo indicado. Véase la página
de manual de
**mview(1)**.

*-V, --version*
: Muestra la versión del programa.

*-x, --xterm*
: Fuerza el modo xterm. Usado cuando se ejecuta en terminales con características de xterm (dos
modos de pantalla, y pueden enviar secuencias de escape de ratón).

*-X, --no-x11*
: No utilizar X11 para obtener el estado de Mayús, Ctrl, Alt.

Si se especifican los dos directorios, el primer nombre se usará para el directorio a mostrar
en el panel activo; el segundo nombre para el directorio a mostrar en el otro panel.

Si solo se especifica un directorio, el nombre se usará para el directorio a mostrar en el
panel activo; el valor de «other_dir» de panels.ini será el nombre del directorio mostrado
en el panel pasivo.

Si no se especifica ningún directorio, el directorio actual se mostrará en el
panel activo; el valor de «other_dir» de panels.ini será el nombre del directorio mostrado
en el panel pasivo.

# Introducción <a id="overview"></a>

La pantalla de M-Commander está divida en cuatro partes. La mayor
parte de la pantalla está ocupada por los dos paneles de directorio. Por defecto,
la segunda línea más inferior de la pantalla es la línea de órdenes del sistema, y
la línea inferior muestra las etiquetas de las teclas de función. La línea superior es la
[barra de menú](#menu-bar).
La línea de la barra de menú podría no ser visible, pero aparece si pulsamos
en la primea línea de la pantalla con el ratón o pulsamos la tecla F9.

M-Commander pone a la vista dos directorios al mismo
tiempo. Uno de los paneles es el panel actual (hay una barra de selección
en el panel actual). La mayoría de las operaciones tienen lugar en el
panel actual. Algunas operaciones con archivos como Renombrar y Copiar utilizan
por defecto el directorio del panel no seleccionado como destino, pero
siempre solicitan una confirmación previa y podemos cambiarlo. Para más
información, ver las secciones sobre los
[Paneles de Directorio](#directory-panels),
los
[Menús Izquierdo y Derecho](#left-and-right-menus)
y el
[Menú de Archivo](#file-menu).

Podemos ejecutar comandos del sistema desde el M-Commander simplemente
escribiéndolos. Todo lo que escribamos aparecerá en la línea de órdenes del sistema
y cuando pulsemos
*Intro,*
M-Commander ejecutará estos comandos; ver las secciones
[Línea de Órdenes del Sistema](#shell-command-line)
y
[Teclas de la Línea de Entrada](#input-line-keys)
para aprender más sobre la línea de órdenes.

# Soporte de Ratón <a id="mouse-support"></a>

Se puede utilizar M-Commander con un ratón o mouse.  Se activa cuando
estamos ejecutándolo en un entorno gráfico con un terminal tipo
**xterm(1)**
(funciona incluso si realizamos una conexión de telnet, ssh o rlogin a
otra máquina desde el xterm) o si estamos ejecutándolo en una consola Linux
y tenemos el servidor
**gpm**
cargado.

Cuando pulsamos el botón izquierdo del ratón sobre un archivo en los paneles
de directorios, ese archivo es seleccionado; si lo hacemos con el botón derecho,
el archivo es marcado (o desmarcado, dependiendo del estado previo).

Una doble pulsación sobre un archivo intentará ejecutar el comando si se trata de
un programa ejecutable; y si la extensión del archivo tiene un programa
[asociado a esa extensión](#edit-extension-file),
se ejecuta el programa especificado.

Además, es posible ejecutar los comandos asignados a las teclas de función
pulsando con el ratón sobre las etiquetas de la línea inferior de la pantalla.

El valor por defecto de auto repetición para los botones del ratón es 400
milisegundos. Este valor se puede modificar editando el archivo
[~/.config/mc6/ini](#save-setup)
y cambiando el parámetro
*mouse_repeat_rate.*

Si estamos ejecutando M-Commander con soporte para ratón, podemos
recuperar el comportamiento habitual del ratón (cortar y pegar texto)
manteniendo pulsada la tecla Mayúsculas.

<!-- help:break -->

# Teclas <a id="keys"></a>

Algunos comandos en M-Commander implican el uso de las teclas
*Control*
(etiquetada habitualmente CTRL o CTL) y
*Meta*
(identificada como ALT o incluso Compose). En este manual usaremos las
siguientes abreviaturas:

**Ctrl-\<car>**
: significa mantener pulsada la tecla Control mientras se pulsa el carácter
\<car>. Así, Ctrl-f sería: manteniendo pulsada la tecla Control teclear f.

**Alt-\<car>**
: significa mantener pulsada la tecla Alt o Meta mientras pulsamos el
carácter \<car>. Si no hay tecla Alt ni Meta, pulsar
*Esc,*
soltar, y entonces pulsar el carácter \<car>.

**Mayús-\<car>**
: significa mantener pulsada la tecla de Mayúsculas (o Shift) y teclear
\<car>.

Todas las líneas de entrada en M-Commander usan una aproximación
a las asociaciones de teclas del editor GNU Emacs.

Se pueden redefinir las asociaciones de las teclas. El resto de los
comportamientos de las teclas que se describen aquí hacen referencia al
comportamiento original. Para más información,
véase la sección sobre
[redefinición de teclas .](#keys_redefine)

Hay bastantes secciones que hablan acerca de las teclas. Las siguientes
son las más importantes.

La sección
[Menú de Archivo](#file-menu)
documenta los atajos de teclado para los comandos que aparecen en
el Menú de Archivo. Esta sección incluye las teclas de función. La mayor parte
de esos comandos realizan alguna acción, normalmente sobre el archivo seleccionado
o sobre los archivos marcados.

La sección
[Paneles de Directorio](#directory-panels)
documenta las teclas que seleccionan un archivo o marcan archivos como
objetivo de una acción posterior (la acción normalmente es una del
menú de archivo).

La sección
[Línea de Órdenes del Sistema](#shell-command-line)
lista las teclas que son usadas para introducir o editar líneas de
comandos. La mayor parte de ellas copian nombres de archivos y demás desde
los paneles de directorio a la línea de órdenes (para evitar un tecleado excesivo)
o acceden al historial de la línea de órdenes.

[Teclas de línea de Entrada](#input-line-keys)
Son usadas para editar líneas de entrada. Esto implica la línea de órdenes
y las líneas de entrada en las ventanas de preguntas.

## Redefinición de teclas <a id="keys_redefine"></a>

La función de ciertas teclas se puede alterar a partir de un mapa de teclado
almacenado en un archivo externo. Inicialmente el programa asigna esas funciones
según el mapa definido en el código fuente. Posteriormente se cargan siempre
los archivos
**{{pkgdatadir}}/keymap.ini**
y
**{{sysconfdir}}/mcommander/keymap.ini**,
reasignando en el orden marcado las definiciones anteriores. Se cargan después
posibles mapas de teclado creados por el usuario atendiendo por orden de prioridad
a:

> 1) Opción de ejecución en línea de órdenes
> **-K \<mapa>**
> o
> **--keymap=\<mapa>**  
> 2) Variable de entorno
> **MC_KEYMAP**  
> 3) Parámetro
> **keymap**
> en la sección
> **[Midnight-Commander]**
> del archivo de configuración.  
> 4) Archivo
> **~/.config/mc6/keymap.ini**  

La opción de línea de órdenes, la variable de entorno y el parámetro en el
archivo de configuración pueden proporcionar la ruta absoluta al archivo
de mapa de teclado (con o sin la extensión .keymap). En caso contrario
se procede a realizar una búsqueda por directorios hasta encontrarlo en:

> 1)
> **~/.config/mc6**  
> 2)
> **{{sysconfdir}}/mcommander/**  
> 3)
> **{{pkgdatadir}}/**

## Otras Teclas <a id="miscellaneous-keys"></a>

Se incluyen aquí las teclas que no encajan en ninguna categoría concreta:

**Intro.**
Si hay algún texto en la línea de órdenes (la de la parte inferior de
los paneles), entonces ese comando es ejecutado. Si no hay texto en la línea
de comandos entonces si la barra de selección está situada sobre un directorio
M-Commander realiza un
**chdir(2)**
al directorio seleccionado y recarga la información en el panel;
si la selección es un archivo ejecutable entonces es ejecutado. Por último,
si la extensión del archivo seleccionado coincide con una de las
extensiones en el
[archivo de extensiones](#edit-extension-file)
entonces se ejecuta la aplicación correspondiente.

**Ctrl-l**
: redibuja toda la pantalla de M-Commander.

**Ctrl-x c**
: [Cambiar permisos](#chmod)
de un archivo o un conjunto de archivos marcados.

**Ctrl-x o**
: [Cambiar dueño](#chown)
del archivo actual o de los archivos marcados.

**Ctrl-x l**
: crea enlaces.

**Ctrl-x s**
: crea enlaces simbólicos con rutas absolutas.

**Ctrl-x v**
: crea enlaces simbólicos con rutas relativas. Para más información
sobre enlaces simbólicos véase la sección
[Menú de Archivo](#file-menu).

**Ctrl-x Ctrl-s**
: edita enlaces simbólicos.

**Ctrl-x i**
: cambia el panel opuesto al modo de información.

**Ctrl-x q**
: cambia el panel opuesto al modo de vista rápida.

**Ctrl-x !**
: ejecuta
[búsquedas externas](#external-panelize).

**Ctrl-x h**
: añade el sitio actual a la lista de
[favoritos](#hotlist).

**Alt-!**
: ejecuta una orden del sistema y muestra su salida en el
[visor de archivos](#internal-file-viewer).

**Alt-?**
: [buscar archivos](#find-file).

**Alt-c**
: permite
[cambiar de directorio](#quick-cd).

**Ctrl-o**
: en la consola de Linux o FreeBSD o bajo un xterm, se muestra la salida
de la orden anterior. En la consola de Linux, M-Commander usa un
programa externo (cons.saver) para controlar la copia y restauración de
la pantalla.

Cuando se haya creado M-Commander con soporte de subshell
incluido, podemos pulsar
*Ctrl-o*
en cualquier momento y volver a la pantalla principal;
para volver a nuestra aplicación bastará con volver a pulsar
*Ctrl-o.*
Si tenemos una aplicación suspendida en esta situación, no podremos
ejecutar otros programas desde M-Commander hasta que terminemos
la aplicación suspendida.

## Paneles de Directorio <a id="directory-panels"></a>

Esta sección enumera las teclas que operan en los paneles de directorio.
Si queremos saber cómo cambiar la apariencia de los paneles, deberemos
echar un vistazo a la sección
[Menús Izquierdo y Derecho](#left-and-right-menus).

**Tab, Ctrl-i**
: cambia el panel actual. El panel activo deja de serlo y el no activo
pasa a ser el nuevo panel activo. La barra de selección se mueve del
antiguo panel al nuevo, desaparece de aquel y aparece en este.

**Insertar, Ctrl-t**
: para marcar archivos (y/o directorios) como seleccionados podemos usar
la tecla
*insertar*
(secuencia kich1 de terminfo).  Para deseleccionar,
basta repetir la operación sobre los archivos y/o directorios antes
marcados.

**Alt-e**
: permite mostrar nombres en el panel con otra codificación de caracteres.
Los nombres se convierten a la codificación del sistema para mostrarlos.
Para desactivar esta recodificación basta seleccionar la entrada (..)
para el directorio superior.  Para cancelar las conversiones en cualquier
directorio seleccionar
*«Sin traducción»*
en el diálogo de selección de código.

**Alt-g, Alt-r, Alt-j**
: usadas para seleccionar el archivo superior en un panel, el archivo central y el inferior del
panel, respectivamente.

**Alt-t**
: rota el listado de pantalla actual para mostrar el siguiente modo
de listado. Con esto es posible intercambiar rápidamente de un listado
completo al regular o breve, así como al modo de listado definido por el usuario.

**Ctrl-\\ (control-Contrabarra)**
: muestra la lista de sitios
[Favoritos](#hotlist)
y permite cambiar al directorio seleccionado.

**\* N. del T.:**
: En el teclado castellano, existe un pequeño inconveniente, dado que la
contrabarra, no se consigue con una sola pulsación, por lo que este
método no funciona directamente.

**+  (más)**
: usado para seleccionar (marcar) un grupo de archivos. M-Commander
ofrecerá distintas opciones.  Indicando
*Solo archivos*
los directorios no se seleccionan.  Con los
*Caracteres Comodín*
habilitados, se pueden introducir expresiones regulares del tipo empleado en
los patrones de nombres de la shell (poniendo \* para cero o más caracteres y ?
para uno o más caracteres). Si los
*Caracteres Comodín*
están deshabilitados, entonces la selección de archivos se realiza con expresiones
regulares normales. Véase la página de manual de
**ed (1)**.
Finalmente, si no se activa
*Distinguir May/min*
la selección se hará sin distinguir caracteres en mayúsculas o minúsculas.

**- (menos) o \\ (contrabarra)**
: usaremos las teclas «-» o «\\» para deseleccionar un grupo de archivos.  Esta es
la operación opuesta a la realizada por la tecla «+».

**\* N. del T.:**
: La tecla que realiza originalmente la función descrita es la «-» (menos)
ya que es la utilizada en la aplicación originaria, Comandante Norton.

**Arriba, Ctrl-p**
: desplaza la barra de selección a la entrada anterior en el panel.

**Abajo, Ctrl-n**
: desplaza la barra de selección a la entrada siguiente en el panel.

**Inicio, Alt-<**
: desplaza la barra de selección a la primera entrada en el panel.

**Fin, Alt->**
: desplaza la barra de selección a la última entrada en el panel.

**AvPág (Página adelante), Ctrl-v**
: desplaza la barra de selección a la página siguiente.

**RePág (Página atrás), Alt-v**
: desplaza la barra de selección a la página anterior.

**Alt-o**
: si el otro panel es un panel con lista de archivos y estamos situados en un
directorio en el panel activo actual, entonces otro panel se posiciona
dentro del directorio del panel activo (como la tecla de Emacs
*Ctrl-o)*
en otro caso el otro panel es posicionado el directorio padre
del directorio seleccionado en el panel activo.

**Alt-i**
: cambiar el directorio en el panel opuesto de manera que coincida con el
panel actual.  Si es necesario se cambiará también el panel opuesto a modo
listado, pero si el panel actual no está en modo listado no se cambiará
de modo el otro.

**Ctrl-RePág, Ctrl-AvPág**
: solamente bajo la consola Linux: realiza un chdir ".." o al
directorio actualmente seleccionado respectivamente.

**Alt-y**
: cambia al anterior directorio visitado, equivale a pulsar
*<*
con el ratón.

**Alt-u**
: cambia al siguiente directorio visitado, equivale a pulsar
*>*
con el ratón.

**Alt-Mayús-h, Alt-H**
: muestra el historial de directorios visitados, equivale a pulsar la
*v*
con el ratón.

## Búsqueda rápida <a id="quick-search"></a>

El modo de Búsqueda rápida permite localizar rápidamente nombres de archivos en
los paneles de directorio. Pulsando
*Ctrl-s*
o
*Alt-s*
se inicia la búsqueda de un archivo en el panel activo.

Estando activada la búsqueda, las teclas pulsadas se van añadiendo a la cadena
de texto en búsqueda y no a la línea de órdenes. Si la opción
*Mostrar Mini-estado*
está habilitada, la cadena a buscar se podrá ver en la línea de estado.
Conforme tecleemos, dentro del panel activo la barra de selección se desplazará
al siguiente archivo o directorio cuyo nombre coincida con las letras
introducidas.  Se pueden usar las teclas
*borrar*
o
*suprimir*
para corregir errores de escritura.  Si pulsamos
*Ctrl-s*
de nuevo, se busca la siguiente coincidencia.

Si se inicia la búsqueda rápida pulsando dos veces
*Ctrl-s*
se recuperará el último patrón de búsqueda utilizado.

Aparte de los caracteres propios de los nombres se pueden utilizar también los
caracteres comodín '\*' y '?'.

## Línea de Órdenes del Sistema <a id="shell-command-line"></a>

Esta sección enumera las teclas útiles para evitar la excesiva escritura
cuando se introducen órdenes del sistema.

**Alt-Intro**
: copia el nombre de archivo seleccionado a la línea de órdenes.

**Ctrl-Intro**
: igual que
*Alt-Intro.*
Puede no funcionar en ciertos sistemas o con algunos terminales.

**Ctrl-Mayús-Intro**
: copia la ruta completa del archivo actual en la línea de órdenes. Puede
no funcionar en ciertos sistemas o con algunos terminales.

**Alt-Tab**
: realiza una
[terminación automática](#completion)
del nombre de archivo, comando, variable, nombre de usuario y host.

**Ctrl-x t, Ctrl-x Ctrl-t**
: copia los archivos marcados (o si no los hay, el archivo
seleccionado) del panel activo (Ctrl-x t) o del otro panel (Ctrl-x Ctrl-t) a
la línea de órdenes.

**Ctrl-x p, Ctrl-x Ctrl-p**
: la primera secuencia de teclas copia el nombre de la ruta de acceso actual
a la línea de órdenes, y la segunda copia la ruta del otro panel a la
línea de órdenes.

**Ctrl-q**
: el comando cita (quote) puede ser utilizado para insertar caracteres
que de otro modo serían interpretados por M-Commander (como el símbolo '+')

**Alt-p, Alt-n**
: usaremos esas teclas para navegar a través del histórico de comandos. Alt-p devuelve
la última entrada, Alt-n devuelve la siguiente.

**Alt-h**
: visualiza el historial para la línea de entrada actual.

## Teclas Generales de Movimiento <a id="general-movement-keys"></a>

El visor de ayuda, el visor de archivo y el árbol de directorios usan
un código de control de movimiento común. Por consiguiente, reconocen las
mismas teclas. Además, cada uno reconoce algunas otras teclas propias.

Otras partes de M-Commander utilizan algunas de las mismas
teclas de movimiento, por lo que esta sección podría ser aplicada a ellas también.

**Arriba, Ctrl-p**
: mueve una línea hacia arriba.

**Abajo, Ctrl-n**
: mueve una línea hacia abajo.

**RePág (Página atrás), Alt-v**
: mueve una página completa hacia atrás.

**AvPág (Página adelante), Ctrl-v**
: mueve una página hacia delante.

**Inicio**
: mueve al principio.

**Fin**
: mueve al final.

El visor de ayuda y el de archivo reconocen las siguientes teclas
aparte de las mencionadas anteriormente:

**b, Ctrl-b, Ctrl-h, Borrar, Suprimir**
: mueve una página completa hacia atrás.

**Barra espaciadora**
: mueve una página hacia delante.

**u, d**
: mueve la mitad de la página hacia atrás o adelante.

**g, G**
: mueve al principio o al final.

## Teclas de la Línea de Entrada <a id="input-line-keys"></a>

Las líneas de entrada (usadas en la
[línea de órdenes](#shell-command-line)
y para los cuadros de diálogo en el programa) reconocen esas teclas:

**Ctrl-a**
: coloca el cursor al comienzo de la línea.

**Ctrl-e**
: coloca el cursor al final de la línea.

**Ctrl-b, Izquierda**
: desplaza el cursor una posición a la izquierda.

**Ctrl-f, Derecha**
: desplaza el cursor una posición a la derecha.

**Alt-f**
: avanza una palabra.

**Alt-b**
: retrocede una palabra.

**Ctrl-h, Borrar**
: borra el carácter anterior.

**Ctrl-d, Suprimir**
: elimina el carácter de la posición del cursor.

**Ctrl-@**
: sitúa una marca para cortar.

**Ctrl-w**
: copia el texto entre el cursor y la marca a la caché de eliminación y elimina
el texto de la línea de entrada.

**Alt-w**
: copia el texto entre el cursor y la marca a la caché de eliminación.

**Ctrl-y**
: restaura el contenido de la caché de eliminación.

**Ctrl-k**
: elimina el texto desde el cursor hasta el final de la línea.

**Alt-p, Alt-n**
: usaremos esas teclas para desplazarnos a través del historial de comandos. Alt-p nos lleva
a la última entrada, Alt-n nos sitúa en la siguiente.

**Ctrl-Alt-h, Alt-Borrar**
: borra la palabra anterior.

**Alt-Tab**
: realiza una
[terminación](#completion)
del nombre de archivo, comando, variable, nombre de usuario o host.

<!-- help:break -->

# Barra de Menú <a id="menu-bar"></a>

La barra de menú aparece cuando pulsamos F9 o pulsamos el botón del ratón
sobre la primera fila de la pantalla. La barra de menú tiene cinco submenús: "Izquierdo", "Archivo",
"Utilidades", "Opciones" y "Derecho".

Los
[Menús Izquierdo y Derecho](#left-and-right-menus)
nos permiten modificar la apariencia de los paneles de directorio
izquierdo y derecho.

El
[Menú de Archivo](#file-menu)
lista las acciones que podemos realizar sobre el archivo actualmente seleccionado
o sobre los archivos marcados.

El
[Menú de Utilidades](#command-menu)
lista las acciones más generales y que no guardan relación con
la selección actual de archivos.

## Menús Izquierdo y Derecho (Arriba y Abajo) <a id="left-and-right-menus"></a>

La presentación de los paneles de directorio puede ser cambiada desde los menús
**Izquierdo**
y
**Derecho**
(denominados
**Arriba**
y
**Abajo**
si hemos elegido la disposición horizontal de paneles en las opciones de
[presentación](#layout)).

### Listado... <a id="listing-format"></a>

La vista en modo
**Listado**
se usa para mostrar la lista de archivos. Hay cuatro modos disponibles:
**Completo**,
**Breve**,
**Largo**,
y
**Definido por el usuario**.

En modo completo se muestra el nombre del archivo, su tamaño y la fecha
y hora de modificación.

En modo breve se muestran solo los nombres de archivo usando entre 1 y 9
columnas. Esto permite ver muchas más entradas que en los otros modos.

El modo largo es similar a la salida de la orden
**ls -l**.
Este modo requiere todo el ancho de la pantalla.

Si se elige el modo definido por el usuario, hay que especificar el
formato de presentación. Un formato personalizado tiene que comenzar con
la indicación de tamaño de panel, que puede ser "half" (medio) o "full"
(completo) para tener respectivamente dos paneles de media pantalla o
un único panel a pantalla completa. Tras el tamaño se puede colocar el
número "2" para dividir el panel en dos columnas.

A continuación van los campos deseados con especificación opcional del
tamaño. Los campos que se pueden emplear son:

**name**
: nombre del archivo.

**size**
: tamaño del archivo.

**bsize**
: forma alternativa para
**size**.
Muestra el tamaño de los archivos y SUB-DIR o DIR-ANT para directorios.

**type**
: carácter de tipo de archivo. Este carácter se asemeja a lo mostrado por
la orden
**ls -F**:
**\***
para archivos ejecutables,
**/**
para directorios,
**@**
para enlaces,
**=**
para sockets,
**-**
para los dispositivos en modo carácter,
**+**
para dispositivos en modo bloque,
**|**
para tuberías,
**~**
para enlaces simbólicos a directorios y
**!**
para enlaces rotos (enlaces que no apuntan a nada).

**mark**
: un asterisco si el archivo está marcado, o un espacio si no lo está.

**mtime**
: fecha y hora de la última modificación del contenido del archivo.

**atime**
: fecha y hora del último acceso al archivo.

**ctime**
: fecha y hora del último cambio del archivo.

**perm**
: cadena representando los permisos del archivo.

**mode**
: valor en octal representando los permisos del archivo.

**nlink**
: número de enlaces al archivo.

**ngid**
: Identificador de Grupo, GID (numérico).

**nuid**
: Identificador de Usuario, UID (numérico).

**owner**
: propietario del archivo.

**group**
: grupo del archivo.

**inode**
: número de inodo del archivo.

Además, podemos ajustar la apariencia del panel con:

**space**
: un espacio.

**|**
: añadir una línea vertical.

Para fijar el tamaño de un campo basta añadir
**:**
seguido por el número de caracteres que se desee. Si tras el número
colocamos el símbolo
**+**
el tamaño indicado será el tamaño mínimo, y si hay espacio de sobra se
extenderá más el campo.

Como ejemplo, el listado
**Completo**
corresponde al formato:

half type name | size | mtime

Y el listado
**Largo**
corresponde a:

full perm space nlink space owner space group space size space mtime
space name

Este es un bonito formato de pantalla definido por el usuario:

half name | size:7 | type mode:3

Los paneles admiten además los siguientes modos:

**Información**
: La vista de información muestra detalles relativos al archivo seleccionado
y, si es posible, sobre el sistema de archivos usado.

**Árbol**
: La vista en árbol es bastante parecida a la utilidad
[árbol de directorios](#directory-tree).
Para más información véase la sección correspondiente.

**Vista Rápida**
: En este modo, en el panel aparece un
[visor](#internal-file-viewer)
reducido que muestra el contenido del archivo seleccionado.  Si se activa
el panel (con el tabulador o con el ratón), se dispone de los funciones
usuales del visor.

### Modo de Ordenación... <a id="sort-order"></a>

Los ocho modos de ordenación son por nombre, por extensión, por hora de modificación,
por hora de acceso, por la hora de modificación de la información del inodo, por tamaño,
por inodo y desordenado. En el cuadro de diálogo del modo de ordenación podemos elegir
el modo de ordenación así como especificar si deseamos que este se realice en orden inverso
chequeando la casilla Invertir.

Por defecto, los directorios se colocan ordenados antes que los archivos.
Esto se puede cambiar en Configuración dentro del
[Menú de Opciones](#options-menu)
activando la opción
**Mezclar archivos y directorios**.

### Filtro... <a id="filter"></a>

La utilidad filtro nos permite seleccionar con un patrón (por ejemplo
**\*.tar.gz**)
los archivos a listar. Indiferentes al patrón de filtro, siempre se
muestran todos los directorios y enlaces a directorios.

### Releer <a id="reread"></a>

El comando releer recarga la lista de archivos en el directorio. Esto es
útil si otros procesos han creado, borrado o modificado archivos. Si
hemos panelizado los nombres de los archivos en un panel, esto recargará
los contenidos del directorio y eliminará la información panelizada.
Véase la sección
[Búsquedas externas](#external-panelize)
para más información.

## Menú de Archivo <a id="file-menu"></a>

M-Commander utiliza las teclas de función
*F1*
\-
*F10*
como atajos de teclado para los comandos que aparecen en el menú de
Archivo. Las secuencias de escape para las Fkeys son características de
terminfo desde kf1 hasta kf10. En terminales sin soporte de teclas de
función, podemos conseguir la misma funcionalidad pulsando la tecla
*Esc*
seguido de un número entre 1 y 9 ó 0 (correspondiendo a las teclas
*F1*
a
*F9*
y
*F10*
respectivamente).

El menú de Archivo recoge las siguientes opciones (con los atajos de
teclado entre paréntesis):

**Ayuda (F1)**

Invoca el visor hipertexto de ayuda interno. Dentro del
[visor de ayuda](#contents),
podemos usar la tecla
*Tab*
para seleccionar el siguiente enlace y la tecla
*Intro*
para seguir ese enlace. Las teclas
*Espacio*
y
*Borrar*
son usadas para mover adelante y atrás en una página de ayuda. Pulsando
*F1*
de nuevo para obtener la lista completa de teclas válidas.

**Menú de Usuario (F2)**

Invoca el
[Menú de usuario](#edit-menu-file)
El menú de usuario otorga una manera fácil de tener usuarios con un menú
y añadir asimismo características extra a M-Commander.

**Ver (F3, Mayús-F3)**

Visualiza el archivo seleccionado. Por defecto invoca el
[Visor de Archivos Interno](#internal-file-viewer)
pero si la opción "Usar visor interno" está desactivada, invoca un visor
de archivos externo especificado por la variable de entorno
**VIEWER.**
Si
**VIEWER**
no está definida se aplica la variable
**PAGER**
y si esta tampoco, se invoca al comando «view».  Con Mayús-F3, se abre
directamente el visor interno, pero sin realizar ningún tipo de formateo
o preprocesamiento del archivo.

Véanse los
[parámetros para el visor externo](#parameters-for-external-editor-or-viewer)
para saber cómo proporcionar opciones adicionales en línea de órdenes
para visores externos.

**Ejecutar y Ver (Alt-!)**

El comando con los argumentos indicados se ejecuta, y la salida se
muestra usando el visor de archivos interno. Como argumento se ofrece,
por defecto, el nombre seleccionado en el panel.

**Editar (F4)**

Invoca el editor
**vi**,
u otro especificado en la variable de entorno
**EDITOR**,
o el
[Editor de Archivos Interno](#internal-file-editor)
si la opción
*use_internal_edit*
está activada.

Véanse los
[parámetros para el editor externo](#parameters-for-external-editor-or-viewer)
para saber cómo proporcionar opciones adicionales en línea de órdenes
para ediotres externos.

**Copiar (F5)**

Sobreimpresiona una ventana de entrada con destino por defecto al directorio del
panel no seleccionado y copia el archivo actualmente seleccionado (o
los archivos marcados, si hay al menos uno marcado) al directorio especificado
por el usuario en la ventana. Space for destination file may be preallocated
relative to preallocate_space configure option. Durante este proceso, podemos
pulsar
*Ctrl-c* o *Esc*
para anular la operación. Para más detalles sobre la máscara de origen
(que será normalmente \* o ^\\(.\*\\)$ dependiendo
de la selección de Uso de los patrones del shell) y los posibles comodines en destino
véase
[Máscara copiar/renombrar](#mask-copyrename).

En algunos sistemas, es posible hacer la copia en segundo plano pulsando en el botón
de segundo plano con el ratón (o pulsando
*Alt-b*
en el cuadro de diálogo). Los
[Trabajos en Segundo Plano](#background-jobs)
son utilizados para controlar los procesos en segundo plano.

**Crear Enlace (Ctrl-x l)**

Crea un enlace al archivo actual.

**Crear Enlace Simbólico (Ctrl-x s)**

Crea un enlace simbólico al archivo actual.  Un enlace es como una copia
del archivo, salvo que el original y el destino representan un único
archivo físico, los mismos datos reales. En consecuencia, si editamos
cualquiera de los archivos, los cambios que realicemos aparecerán en
todos los archivos. Reciben también el nombre de alias o accesos
directos.

Un enlace aparece como un archivo real. Después de crearlo, no hay modo
de decir cuál es el original y cuál el enlace. Si borramos uno de ellos
el otro aún seguirá intacto. Es muy difícil advertir que los archivos
representan la misma imagen. Usaremos estos enlaces cuando no
necesitemos saberlo.

Un enlace simbólico es, en cambio, solo una referencia al nombre del
archivo original.  Si se borra el archivo original, el enlace simbólico
queda sin utilidad. Es bastante fácil advertir que los archivos
representan la misma imagen. M-Commander muestra un símbolo "@"
delante del nombre del archivo si es un enlace simbólico a alguna parte
(excepto a un directorio, caso en que muestra una tilde (~)). El archivo
original al cual apunta el enlace se muestra en la línea de estado si la
opción
*Mostrar Mini-estado*
está habilitada. Usaremos enlaces simbólicos cuando queramos evitar la
confusión que pueden causar los enlaces físicos.

**Renombrar/Mover (F6)**

Presenta un diálogo de entrada proponiendo como directorio de destino el
directorio del panel no activo, y mueve allí, o bien los archivos marcados
o en su defecto el archivo seleccionado. El usuario puede introducir en
el diálogo un destino diferente. Durante el proceso, se puede pulsar
*Ctrl-c* o *Esc*
para abortar la operación. Para más detalles, véase más arriba la
operación Copiar, dado que la mayoría de los aspectos son similares.

En algunos sistemas, es posible hacer la copia en segundo plano pulsando
con el ratón en el susodicho botón de segundo plano (o pulsando
*Alt-o*
en el cuadro de diálogo). Con
[Procesos en 2º plano](#background-jobs)
se puede controlar estas tareas.

**Crear Directorio (F7)**

Presenta un diálogo de entrada y crea el directorio especificado.

**Borrar (F8)**

Borra, o bien los archivos marcados o en su defecto el archivo
seleccionado en el panel activo. Durante el proceso, se puede pulsar
*Ctrl-c* o *Esc*
para abortar la operación.

**Cambiar Directorio (Alt-c)**
Usaremos el comando
[Cambiar de directorio](#quick-cd)
si tenemos llena la línea de órdenes y queremos hacer un cd a algún lugar.

**Seleccionar Grupo (+)**

Se utiliza para seleccionar (marcar) un grupo de archivos.  M-Commander
ofrecerá distintas opciones.  Indicando
*Solo archivos*
los directorios no se seleccionan.  Con los
*Caracteres Comodín*
habilitados, se pueden introducir expresiones regulares del tipo empleado en
los patrones de nombres de la shell (poniendo \* para cero o más caracteres y ?
para uno o más caracteres).  Si los
*Caracteres Comodín*
están deshabilitados, entonces la selección de archivos se realiza con expresiones
regulares normales. Véase la página de manual de
**ed (1)**.
Finalmente, si no se activa
*Distinguir May/min*
la selección se hará sin distinguir caracteres en mayúsculas o minúsculas.

**De-seleccionar Grupo (\\)**

Utilizado para deseleccionar un grupo de archivos. Es la operación antagonista al comando
*Selecciona grupo.*

**Salir (F10, Mayús-F10)**

Finaliza M-Commander. Mayús-F10 es usado cuando queremos
salir y estamos utilizando la envoltura del shell. Mayús-F10 no nos llevará
al último directorio visitado con M-Commander, en vez de eso
nos llevará al directorio donde fue invocado M-Commander.

### Cambiar de directorio <a id="quick-cd"></a>

Este comando es útil si tenemos completa la línea de órdenes y
queremos hacer un
[cd](#the-cd-internal-command)
a algún lugar sin tener que cortar y pegar sobre la línea. Este comando
sobreimpresiona una pequeña ventana, donde introducimos todo aquello que
es válido como argumento del comando
**cd**
en la línea de órdenes y después pulsamos intro. Este comando caracteriza
todas las cualidades incluidas en el
[comando cd interno](#the-cd-internal-command).

## Menú de Utilidades <a id="command-menu"></a>

[Árbol de directorios](#directory-tree)
muestra una figura con estructura de árbol con los directorios.

[Buscar archivos](#find-file)
permite buscar un archivo específico. El comando "Intercambiar paneles"
intercambia los contenidos de los dos paneles de directorios.

El comando "Activa/desactiva paneles" muestra la salida del último
comando del shell. Esto funciona solo en xterm y en una consola Linux y
FreeBSD.

El comando Compara directorios (Ctrl-x d) compara los paneles de directorio
uno con el otro. Podemos usar el comando Copiar (F5) para hacer ambos
paneles idénticos. Hay tres métodos de comparación. El método rápido
compara solo el tamaño de archivo y la fecha. El método completo realiza
una comparación completa octeto a octeto. El método de comparación
de solo tamaño solo compara los tamaños de archivo y no chequea los
contenidos o las fechas, solo chequea los tamaños de los archivos.

El comando Histórico de comandos muestra una lista
de los comandos escritos. El comando seleccionado es copiado a la línea de órdenes.
El histórico de comandos puede ser accedido también tecleando Alt-p ó Alt-n.

[Favoritos](#hotlist) (Ctrl-\\)
permite acceder con facilidad a directorios y sitios utilizados con frecuencia.

[Búsquedas Externas](#external-panelize)
nos permite ejecutar un programa externo, y llevar la salida de ese
programa al panel actual.

[Editar el archivo de extensiones](#edit-extension-file)
nos permite especificar los programas a ejecutar para intentar
ejecutar, ver, editar y realizar un montón de cosas sobre archivos
con ciertas extensiones (terminaciones de archivo). Por ejemplo, asociar la extensión
de los archivos de audio de SUN (.au) con el programa reproductor adecuado.
[Editar archivo de menú](#edit-menu-file)
se puede utilizar para editar el menú de usuario (el que aparece al
pulsar F2).

### Árbol de Directorios <a id="directory-tree"></a>

El comando Árbol de directorios muestra una figura con la estructura de los directorios.
Podemos seleccionar un directorio de la figura y M-Commander cambiará
a ese directorio.

Hay dos modos de invocar el árbol. El comando de árbol de directorios
está disponible desde el menú Utilidades. El otro modo es seleccionar la vista en árbol
desde el menú Izquierdo o Derecho.

Para evitar largos retardos M-Commander crea la figura de árbol
escaneando solamente un pequeño subconjunto de todos los directorios. Si
el directorio que queremos ver no está, nos moveremos hasta su directorio padre
y pulsaremos Ctrl-r (o F2).

Podemos utilizar las siguientes teclas:

[Teclas de Movimiento General](#general-movement-keys)
válidas.

**Intro.**
En el árbol de directorios, sale del árbol de directorios y cambia al
directorio en el panel actual. En la vista de árbol, cambia a este directorio
en el otro panel y permanece en el modo de vista Árbol en el panel actual.

**Ctrl-r, F2 (Releer).**
Relee este directorio. Usaremos este comando cuando el árbol de directorios esté anticuado:
hay directorios perdidos o muestra algunos directorios que no existen ya.

**F3 (Olvidar).**
Borra ese directorio de la figura del árbol. Usaremos esto para eliminar
desorden de la figura. Si queremos que el directorio vuelva a la figura del árbol
pulsaremos F2 en su directorio padre.

**F4 (Estático/Dinámico, Dinam/Estát).**
Intercambia entre el modo de navegación dinámico (predefinido) y el modo estático.

En el modo de navegación estático podemos usar las teclas del cursor Arriba y Abajo
para seleccionar un directorio. Todos los directorios conocidos serán mostrados.

En el modo de navegación dinámico podemos usar las teclas del cursor Arriba y Abajo
para seleccionar el directorio hermano, la tecla Izquierda para situarnos en el directorio
padre, y la tecla Derecha para situarnos en el directorio hijo. Solo los directorios
padre, hijo y hermano son mostrados, el resto son dejados fuera. La figura de árbol cambia
dinámicamente conforme nos desplazamos sobre ella.

**F5 (Copiar).**
Copia el directorio.

**F6 (Renombrar/Mover, RenMov).**
Mueve el directorio.

**F7 (Mkdir).**
Crea un nuevo directorio por debajo del directorio actual. El directorio creado
será así el hijo del directorio del cual depende jerárquicamente (Padre).

**F8 (Eliminar).**
Elimina este directorio del sistema de archivos.

**Ctrl-s, Alt-s.**
Busca el siguiente directorio coincidente con la cadena de búsqueda. Si no hay
tal directorio esas teclas moverán una línea abajo.

**Ctrl-h, Borrar.**
Borra el último carácter de la cadena de búsqueda.

**Cualquier otro carácter.**
Añade el carácter a la cadena de búsqueda y se desplaza al siguiente directorio
que comienza con esos caracteres. En la vista de árbol debemos primero
activar el modo de búsqueda pulsando
*Ctrl-s.*
La cadena de búsqueda se muestra en la línea de estado.

Las siguientes acciones están disponibles solo en el árbol de directorios. No
son funcionales en la vista de árbol.

**F1 (Ayuda).**
Invoca el visor de ayuda y muestra esta sección.

**Esc, F10.**
Sale del árbol de directorios. No cambia el directorio.

El ratón es soportado. Un
*doble click*
se comporta como pulsar
*Intro.*
Véase también la sección sobre
[soporte de ratón](#mouse-support).

### Buscar Archivos <a id="find-file"></a>

La utilidad para Buscar Archivos primero pregunta por el directorio de inicio
y el nombre de archivo a buscar.  Pulsando el botón Árbol podemos seleccionar
el directorio inicial en el
[Árbol de directorios](#directory-tree).

El campo de contenidos puede aceptar expresiones regulares similares a egrep(1). En
ese caso podremos proteger caracteres con significado especial para egrep anteponiendo «\\»,
p.ej. si buscamos «strcmp (» tendremos que introducir «strcmp \\(".

Con la opción «Palabras completas» se puede limitar la búsqueda a archivos donde
la parte coincidente forme una palabra completa.  Eso se corresponde con la función
de la opción «-w» de grep.

Podemos iniciar la búsqueda pulsando el botón Aceptar.
Durante el proceso de búsqueda podemos detenerla desde el botón Terminar.

Podemos navegar por la lista de archivos con las teclas del cursor Arriba y Abajo. El botón Chdir
cambiará al directorio del archivo actualmente seleccionado. El botón "Otra vez" preguntará los parámetros para una nueva
búsqueda. El botón Terminar finaliza la operación de búsqueda. El botón Panelizar
colocará los archivos encontrados en el panel actual y así
podremos realizar más operaciones con ellos (ver, copiar, mover,
borrar y demás). Después de panelizar podemos pulsar Ctrl-r para regresar al listado
normal de archivos.

Es posible tener una lista de directorios que el comando Buscar Archivo
debería saltar durante la búsqueda (por ejemplo, podemos querer
evitar búsquedas en un CDROM o en un directorio NFS que está montado a través de un
enlace lento).

Los directorios a ser omitidos deberían ser enumerados en la variable
**ignore_dirs**
en la sección
**FindFile**
de nuestro archivo
*~/.config/mc6/ini.*

Los componentes del directorio deberían ser separados por dos puntos, como en
el ejemplo que sigue:

```
[FindFile]
ignore_dirs=/cdrom:/nfs/wuarchive:/afs
```

Debemos valorar la utilización de
[Búsquedas externas](#external-panelize)
en ciertas situaciones. La utilidad Buscar archivos es solo para consultas
simples, pero con Búsquedas externas se pueden hacer exploraciones tan
complejas como queramos.

### Búsquedas Externas <a id="external-panelize"></a>

Búsquedas externas nos permite ejecutar un programa externo, y
tomar la salida de ese programa como contenido del panel actual.

Por ejemplo, si queremos manipular en uno de los paneles todos los enlaces
simbólicos del directorio actual, podemos usar búsquedas externas para
ejecutar el siguiente comando:

```
find . -type l -print
```

Hasta la finalización del comando, el contenido del directorio del panel no
será el listado de directorios del directorio actual, pero sí todos los archivos
que son enlaces simbólicos.

Si queremos panelizar todos los archivos que hemos bajado de nuestro servidor ftp,
podemos usar el comando awk para extraer el nombre del archivo
de los archivos de registro (log) de la transferencia:

```
awk '$9 ~! /incoming/ { print $9 }' < /var/log/xferlog
```

Tal vez podríamos necesitar guardar los comandos utilizados frecuentemente bajo un nombre descriptivo,
de manera que podamos llamarlos rápidamente. Haremos esto tecleando el comando
en la línea de entrada y pulsando el botón "Añadir nuevo". Entonces introduciremos un nombre
bajo el cual queremos que el comando sea guardado. La próxima vez, bastará elegir
ese comando de la lista y no habrá que escribirlo de nuevo.

### Favoritos <a id="hotlist"></a>

Muestra una lista de sitios y directorios guardados y abre en el panel
el lugar seleccionado. Desde el cuadro de diálogo podemos también crear y
eliminar entradas. Para añadir se puede igualmente utilizar Añadir Actual
(Ctrl-x h), que añade el directorio actual (no el seleccionado) a la lista
de favoritos. Se pide al usuario una etiqueta para identificar la entrada.

Esto hace más rápido el posicionamiento en los directorios usados
frecuentemente.  Deberíamos considerar también el uso de la variable
CDPATH tal y como se describe en
[comando cd interno](#the-cd-internal-command).

### Editar el Archivo de Extensiones <a id="edit-extension-file"></a>

Abre el archivo
*~/.config/mc6/extensions.ini*
en nuestro editor.
If this file does not exist and you are not root, it will be copied from
*{{sysconfdir}}/mcommander/extensions.ini.*
If you are root, you can choose the file to edit: user's
*~/.config/mc6/extensions.ini*
or system-wide
*{{sysconfdir}}/mcommander/extensions.ini.*
The format of this file is described in detail in it.

### Trabajos en Segundo Plano <a id="background-jobs"></a>

Nos permite controlar el estado de cualquier proceso de M-Commander
en segundo plano (solo las operaciones de copiar y mover archivos pueden realizarse
en segundo plano). Podemos parar, reiniciar y eliminar procesos en segundo plano desde
aquí.

### Edición del Archivo de Menú <a id="edit-menu-file"></a>

El menú de usuario es un menú de acciones útiles que puede ser personalizado
por el usuario. Cuando accedemos al menú de usuario se utiliza, si existe,
el archivo .usermenu del directorio actual, pero solo si es propiedad del
usuario o del superusuario y no es modificable por todos.
Si no se encuentra allí el archivo, se intenta de la misma manera con ~/.config/mc6/menu,
y si no, mcommander utiliza el menú por defecto para todo el sistema
{{pkgdatadir}}/usermenu.

El formato del menú de archivo es muy simple.  Todas las líneas, salvo
las que empiezan con espacio o tabulación, son consideradas entradas
para el menú (para posibilitar su uso como atajo de teclado, el primer
carácter sí deberá ser una letra).  Las líneas que comienzan con una
tabulación o espacio son los comandos que serán ejecutados cuando la
entrada es seleccionada.

Cuando se selecciona una opción todas las líneas de comandos de esa
opción se copian en un archivo temporal en el directorio temporal
(normalmente /usr/tmp), y se ejecuta ese archivo.  Esto permite al
usuario utilizar en los menús construcciones normales de la shell.
También tiene lugar una sustitución simple de macros antes de ejecutar
el código del menú. Para mayor información, ver
[Sustitución de macro](#macro-substitution).

He aquí un ejemplo de archivo usermenu:

```
A	Vuelca el contenido del archivo seleccionado
	od -c %f

B	Edita un informe de errores y lo envía al superusuario
	I=`mktemp ${MC_TMPDIR:-/tmp}/mail.XXXXXX` || exit 1
	vi $I
	mail -s "Error M-Commander" root < $I
	rm -f $I

M	Lee al correo
	emacs -f rmail

N	Lee las noticias de Usenet
	emacs -f gnus

H	Realiza una llamada al navegador hypertexto info
	info

J	Copia recursivamente el directorio actual al otro panel
	tar cf - . | (cd %D && tar xvpf -)

K	Realiza una versión del directorio actual
	echo -n "Nombre del archivo de distribución: "
	read tar
	ln -s %d `dirname %d`/$tar
	cd ..
	tar cvhf ${tar}.tar $tar

= f *.tar.gz | f *.tgz & t n
X       Extrae los contenidos de un archivo tar comprimido
	tar xzvf %f
```

**Condiciones por Defecto**

Cada entrada del menú puede ir precedida por una condición. La condición debe
comenzar desde la primera columna con un carácter '='. Si la condición es
verdadera, la entrada del menú será la entrada por defecto.

```
Sintaxis condicional: 	= <sub-cond>
  o:			= <sub-cond> | <sub-cond> ...
  o:			= <sub-cond> & <sub-cond> ...

Sub-condición es una de las siguientes:

  f <patrón>		¿el archivo actual encaja con el patrón?
  F <patrón>		¿otro archivo encaja con el patrón?
  d <patrón>		¿el directorio actual encaja con el patrón?
  D <patrón>		¿otro directorio encaja con el patrón?
  t <tipo>		¿archivo actual es de tipo <tipo>?
  T <tipo>		¿otro archivo es de tipo <tipo>?
  ! <sub-cond>		niega el resultado de la sub-condición
```

Patrón es un patrón normal del shell o una expresión regular, de acuerdo
con la opción de patrones del shell. Podemos cambiar el valor global de
la opción de los patrones del shell escribiendo "shell_patterns=x" en la primera línea
del archivo de menú (donde "x" es 0 ó 1).

Tipo es uno o más de los siguientes caracteres:

```
  n	no directorio
  r	archivo regular
  d	directorio
  l	enlace
  c	dispositivo tipo carácter
  b	dispositivo tipo bloque
  f	tubería (fifo)
  s	socket
  x	ejecutable
  t	marcado (tagged)
```

Por ejemplo 'rlf' significa archivo regular, enlace o cola. El tipo 't'
es un poco especial porque actúa sobre el panel en vez de sobre
un archivo. La condición '=t t' es verdadera si existen archivos marcados en el
panel actual y falsa si no los hay.

Si la condición comienza con '=?' en vez de '=' se mostrará un trazado de
depuración mientras el valor de la condición es calculado.

Las condiciones son calculadas de izquierda a derecha. Esto significa que

```
	= f *.tar.gz | f *.tgz & t n
```

es calculado como

```
	( (f *.tar.gz) | (f *.tgz) ) & (t n)
```

He aquí un ejemplo de uso de condiciones:

```
= f *.tar.gz | f *.tgz & t n
L	Lista el contenido de un archivo tar comprimido
	gzip -cd %f | tar xvf -
```

**Condiciones aditivas**

Si la condición comienza con '+' (o '+?') en lugar de '=' (o '=?') es
una condición aditiva. Si la condición es verdadera la entrada de menú será
incluida en el menú. Sin embargo, si la condición es falsa, la entrada de menú no será
incluida en el menú.

Podemos combinar condiciones por defecto y aditivas comenzando la condición con
'+=' o '=+' (o '+=?' o '=+?' si queremos depurar). Si nosotros queremos
condiciones diferentes, una para añadir y otra por
defecto, una entrada de menú con dos líneas de condición, una
comenzando con '+' y otra con '='.

Los comentarios empiezan con '#'. Las líneas adicionales de comentarios deben empezar
con '#', espacio o tabulación.

## Menú de Opciones <a id="options-menu"></a>

M-Commander tiene opciones que pueden ser activadas o desactivadas
a través de una serie de diálogos a los que se accede desde este menú. Una
opción está activada cuando tiene delante un asterisco o una "x".

En
[Configuración](#configuration)
se pueden cambiar la mayoría de opciones de M-Commander.

En
[Presentación](#layout)
está un grupo de opciones que determinan la apariencia de mcommander en la
pantalla.

En
[Paneles](#panel-options)
se pueden configurar los paneles del gestor de archivos.

En
[Confirmación](#confirmation)
podemos especificar qué acciones requieren una confirmación del usuario
antes de ser realizadas.

En
[Aspecto](#appearance)
podemos seleccionar un «skin» o apariencia para el programa.

En
[Aprender Teclas](#learn-keys)
podemos verificar teclas que no funcionan en algunos terminales y
solucionarlo.

En
[Sistema de Archivos Virtual (VFS)](#virtual-fs)
podemos especificar algunas opciones relacionadas con el VFS (Sistema
de Archivos Virtual).

[Guardar Configuración](#save-setup)
guarda los valores actuales de los menús Izquierdo, Derecho y Opciones.
También se guardan algunos otros valores.

### Configuración <a id="configuration"></a>

Este diálogo presenta una serie de opciones divididas en tres grupos:
«Operaciones con Archivos», «Tecla de Escape», «Pausa Después de
Ejecutar» y «Otras Opciones».

**Operaciones con Archivos**

*Operación Detallada.*
Controla la visualización de detalles durante las operaciones de
Copiar, Mover y Borrar (i.e., muestra un cuadro de diálogo para cada
operación). Si tenemos un terminal lento, podríamos querer desactivar
la operación detallada. Se desactiva automáticamente si la velocidad de
nuestro terminal es menor de 9600 bps.

*Calcular Totales.*
Hace que M-Commander calcule el total de bytes y el número de
archivos antes de iniciar operaciones de Copiar, Mover y Borrar. Esto
proporciona una barra de progreso más precisa a costa de cierta
velocidad. Esta opción no tiene efecto si la
*Operación Detallada*
no está seleccionada.

*Barra de Progreso Clásica.*
Con esta opción la barra de progreso para las operaciones de Copiar,
Mover o Borrar avanza de izquierda a derecha. Si se deshabilita, el
sentido de crecimiento refleja el sentido de la copia: del panel
izquierdo al derecho o viceversa. Por defecto, está activa.

*Proponer Nombre Mkdir.*
Al pulsar F7 para crear un directorio nuevo, la línea de entrada
del diálogo incorpora como sugerencia el nombre del archivo o
directorio actual en el panbel activo. Está deshabilitado por defecto.

*Reservar Espacio.*
Antes de comenzar una copia reserva espacio para el archivo destino
completo. Por defecto está desactivado.

**Tecla de Escape.**

M-Commander utiliza la tecla ESC como prefijo para ciertas teclas.
Por ello hay que pulsar ESC dos veces para abandonar los diálogos. Se
puede configurar para que esto se pueda realizar con una única pulsación.
*Pulsación Única*
Por defecto, está deshabilitada. Permite que ESC actúe como prefijo durante
un cierto tiempo (véase abajo la opción
*Tiempo)*
al cabo del cual se interpreta ESC para cancelar (ESC ESC).

*Tiempo.*
Permite configurar el intervalo (en microsegundos) para una pulsación
de ESC autónoma. Por defecto es de un segundo (1000000 microsegundos).
Este intervalo también se puede fijar a través de la variable de entorno
KEYBOARD_KEY_TIMEOUT_US (también en microsegundos) que tiene prioridad
sobre el valor de esta opción Tiempo.

**Pausa Después de Ejecutar.**

Después de ejecutar comandos, M-Commander puede realizar una pausa,
y darnos tiempo a examinar la salida del comando. Hay tres posibles
valores para esta variable:

*Nunca.*
Significa que no queremos ver la salida de nuestros comandos. Si estamos
utilizando la consola Linux o FreeBSD o un xterm, podremos ver la salida
del comando pulsando
*Ctrl-o.*

*SoloenTerminalesTontas.*
Obtendremos el mensaje de pausa solo en terminales que no sean capaces
de mostrar la salida del último comando ejecutado (en realidad, cualquier
terminal que no sea un xterm o una consola de Linux).

*Siempre.*
El programa realizará siempre una pausa después de ejecutar comandos.

**Otras Opciones**

*Usar Editor Interno.*
Emplear el editor de archivos interno. Si está desactivada, se editarán
los archivos con el editor especificado por la variable de entorno
**EDITOR**
y si no se especifica ninguno, se usará
**vi.**
Véase la sección sobre el
[editor de archivos interno](#internal-file-editor).

*Usar Visor Interno.*
Emplear el visor de archivos interno. Si la opción está desactivada,
el paginador especificado en la variable de entorno
**PAGER**
será el utilizado.
Si no se especifica ninguno, se usará el comando
**view.**
Véase la sección sobre el
[visor de archivos interno](#internal-file-viewer).

*Pedir Nombre al Editar Nuevos.*
Si está activada, se pedirá al usuario el nombre de archivo antes de abrir
un archivo nuevo en el editor.

*Auto Menús.*
Si está activada, el menú de usuario aparece automáticamente al arrancar.
Útil en menús construidos para personas sin conocimientos de Unix.

*Menús Desplegables.*
Mostrar el contenido de los menús desplegables inmediatamente al presionar
F9. Si está desactivada solo la barra de títulos de los menús está
visible, y será necesario abrir cada menú con las flechas de movimiento
o con las teclas de acceso rápido.
*Completar: Mostrar Todos.*
Por defecto, al completar nombres en situaciones de ambigüedad,
M-Commander completa todo lo posible al pulsar
**Alt-Tab**
y produce un pitido; al intentarlo por segunda vez se muestra una lista
con las posibilidades que han dado lugar a la ambigüedad. Con esta opción,
la lista aparece directamente tras pulsar
**Alt-Tab**
por primera vez.

*Patrones «shell».*
Por defecto, las funciones Selección, Deselección y Filtro emplean
expresiones regulares al estilo del shell. Para realizar esto se
realizan las siguientes conversiones: '\*' se cambia por '.\*' (cero o
más caracteres); '?' por '.' (exactamente un carácter) y '.' por un
punto literal. Si la opción está desactivada, entonces las expresiones
regulares son las descritas en ed(1).

*Completar: Mostrar Todos.*
Por defecto M-Commander presenta todas las posibilidades de
[terminación](#completion)
si la compleción es ambigua solo al pulsar
**Alt-Tab**
por segunda vez.  La primera, solo completa todo lo posible y emite
un pitido en caso de ambigüedad.  Activando esta opción se muestran
todas las posibilidades directamente con la primera pulsación de
**Alt-Tab**.

*Hélice de actividad.*
Mostrar un guión rotatorio en la esquina superior derecha a modo de
indicador de progreso.

*Cd Sigue los Enlaces.*
Esta opción, si está seleccionada, hace que M-Commander siga la
secuencia de directorios lógica al cambiar el directorio actual, tanto en
el panel como usando el comando cd. Este es el comportamiento por defecto
de la shell bash. Sin esto, M-Commander sigue la estructura real
de directorios, y cd .. nos trasladará al padre real del directorio
actual aunque hayamos entrado en ese directorio a través de un enlace,
y no al directorio donde se encontraba el enlace.

*Precauciones de Borrado.*
Dificulta el borrado accidental de archivos. La opción por defecto en el
diálogo de confirmación de borrado se cambia a "No". Por defecto, esta
opción está desactivada.

*Auto-Guarda Configuración.*
Si esta opción está activada, cuando salimos de M-Commander las
opciones configurables de M-Commander se guardan en el archivo
*~/.config/mc6/ini.*

### Presentación <a id="layout"></a>

La ventana de presentación nos da la posibilidad de cambiar la
presentación general de la pantalla. Podemos configurar si son visibles
la barra de menú, la línea de órdenes, la línea de sugerencias o la
barra de teclas de Función. En la consola Linux o FreeBSD podemos
especificar cuántas líneas se muestran en la ventana de salida.

El resto del área de pantalla se utiliza para los dos paneles de
directorio. Podemos elegir si disponemos los paneles vertical u
horizontalmente. La división puede ser simétrica o bien podemos indicar
una división asimétrica.

Por defecto, todos los contenidos de los paneles se muestran en el mismo
color, pero se puede indicar que
*permisos*
y
*tipos de archivos*
se resalten empleando
[colores](#colors)
diferentes. Si se activa el resaltado de permisos, las partes de los
campos de
*permisos*
del
[Modo de Listado](#listing-format)
aplicables al usuario actual de M-Commander serán resaltados
usando el color indicado por medio de la palabra clave
*selected.*
Si se activa el resaltado de tipos de archivos, los nombres aparecerán
coloreados según las reglas almacenadas en el archivo
*{{pkgdatadir}}/filehighlight.ini.*
Para más información, véase la
sección sobre
[Resaltado de nombres](#filenames-highlight).

Si se está ejecutando en X Window dentro de un emulador de terminal,
M-Commander toma control del titulo de la ventana mostrando allí
el nombre del directorio actual.  El título se actualiza cuando sea preciso.
Podemos desactivar la opción de
*Titular las ventanas Xterm*
si el emulador de terminal empleado falla y no se muestran o actualizan
correctamente estos textos.

### Paneles <a id="panel-options"></a>

**Opciones principales**

*Mostrar Mini-estado*
Si está activa se muestra en la parte inferior de cada panel una línea
con información sobre el archivo seleccionado en cada momento. Por defecto,
está activado.

*Tamaños en unidades SI.*
Mostrar tamaños de archivos en bytes con prefijos según el SI, Sistema
Internacional de Unidades, o sea, en base de 10. Por defecto, está
desactivada: los tamaños se calculan con prefijos según el IEC, empleando
base de 2. Véase al respecto ISO/IEC 80000-13.

*Mezclar Archivos y Directorios.*
Cuando esta opción está habilitada, todos los archivos y directorios
se muestran mezclados. Por defecto esta opción está desactivada: los directorios
(y enlaces a directorios) aparecen al principio de la lista, y el resto
de archivos a continuación.

*Mostrar Archivos de Respaldo.*
Mostrar los archivos terminados en tilde '~'. Si se desactiva no se muestran
(como la opción -B de ls de GNU). Por defecto, está activo.

*Mostrar Archivos Ocultos.*
Mostrar los archivos que comiencen con un punto (como ls -a). Por defecto,
está desactivado.

*Recarga Rápida de Directorios.*
Hace que M-Commander emplee una pequeña trampa al determinar
si los contenidos del directorio han cambiado. El truco consiste en
recargar el directorio solo si el inodo del directorio ha cambiado. Las
recargas se producen si se crean o borran archivos, pero si lo que cambia
es solo el inodo de un archivo del directorio (cambios en el tamaño,
permisos, propietario, etc.) la pantalla no se actualiza. En esos casos,
si tenemos la opción activada, será preciso forzar la recarga de forma
manual (con Ctrl-r). Por defecto, está desactivado.

*Marcar y Avanzar.*
Hacer avanzar la barra de selección tras marcar un archivo (con la tecla
**insertar**).
Por defecto, está activo.

*Invertir Solo Archivos.*
Permite invertir la selección solo sobre los archivos. Por defecto, está activo.
Al invertir la selección se aplica solo a archivos, quedando los directorios
como estaban. Si se desactiva, todos los elementos no seleccionados se seleccionan
y viceversa, sean archivos o directorios.

*Intercambio de Paneles Simple.*
Si los dos paneles contienen listados de directorios, el intercambio simple
supone que ambos paneles intercambian sus posiciones: izquierda por derecha.
Si se desactiva, que es el estado por defecto, los contenidos de los paneles
se intercambian pero se mantienen las opciones de formato y orden de archivos.

*Auto Guardar Configuración*
Por defecto está desactivado. Si se activa, la configuración de los paneles
se guardará en
*~/.config/mc6/panels.ini*
al salir del programa.

**Navegación**

*Navegación al Estilo Lynx.*
Cuando la selección es un directorio y la línea de órdenes está vacía
permite cambiar a él con las flechas de movimiento. Esta opción está
inactiva por defecto.

*Avance de Página.*
Por defecto, cuando el cursor llega al final o al comienzo del panel
este se desplaza el equivalente a media pantalla. Al desactivarlo el
avance o retroceso se hace línea a línea.

*Avance de Página con Ratón.*
Controla si el avance en los paneles con la rueda del ratón se hace
por páginas o por líneas.

**Resaltar**

Permite que los
*permisos*
y
*tipos de archivos*
queden resaltados con
[colores](#colors)
distintivos. Si se habilita el resaltado de permisos, los
[campos del listado](#listing-format)
*perm*
y
*mode*
aplicables al usuario que ejecuta M-Commander se mostrarán destacados en el
color indicado con la clave
*selected.*
Si se habilita el resaltado de tipo de archivo, los nombres de archivo
se mostrarán coloreados según las reglas contenidas en el archivo de
configuración
*{{sysconfdir}}/mcommander/filehighlight.ini.*
Véase
[Resaltado de nombres](#filenames-highlight).

**Búsqueda rápida**

Permite configurar si la
[Búsqueda rápida](#quick-search)
distingue o no mayúsculas en los nombres: ignorar, distinguir o aplicar el mismo
criterio elegido en el orden de los nombres en el panel.

### Confirmación <a id="confirmation"></a>

En este diálogo configuramos las opciones de confirmación de eliminación de archivos,
sobreescritura, ejecución pulsando intro y salir del programa.

### Aspecto <a id="appearance"></a>

Aquí se puede elegir un «skin» o apariencia para usar.

Véase la sección sobre
[Skins](#skins)
para conocer los detalles de los archivos de definición de estos «skins».

### Aprender teclas <a id="learn-keys"></a>

Este diálogo nos permite comprobar si nuestras teclas F1-F20, Inicio, Fin, etc. funcionan adecuadamente
en nuestro terminal. A menudo fallan, dado que muchas bases de datos de terminales están mal.

Podemos movernos alrededor con la tecla Tab, con las teclas de movimiento de vi ('h' izquierda, 'j'
abajo, 'k' arriba y 'l' derecha) y después de pulsar cualquier tecla del cursor (esto
las marcará con OK), entonces podremos usar esa tecla también.

Para probarlas basta con pulsar cada una de ellas. Tan pronto como pulsamos una tecla y
esta funciona adecuadamente, la marca «✓» debería aparecer junto al nombre de la susodicha
tecla. Una vez que cada tecla queda marcada vuelve a funcionar con normalidad, p. ej. F1 la
primera vez comprobará que F1 funciona perfectamente, pero a partir de ese momento mostrará
la ayuda.  Esto mismo es aplicable a las teclas del cursor.  La tecla Tab debería funcionar
siempre.

Si algunas teclas no funcionan adecuadamente, entonces no veremos el OK tras el nombre de la tecla
después de haberla pulsado. Podemos entonces intentar solucionarlo. Haremos esto
pulsando el botón de esa tecla (con el ratón o usando
*Tab e Intro).*
Entonces un mensaje rojo aparecerá y se nos pedirá que pulsemos la tecla en cuestión.
Si deseamos abortar el proceso, bastará con pulsar
*Esc*
y esperar hasta que el mensaje
desaparezca. Si no, pulsaremos la tecla que nos pide y esperaremos hasta que
el diálogo desaparezca.

Cuando acabemos con todas las teclas, podríamos Guardar nuestras teclas
en nuestro archivo
*~/.config/mc6/ini*
dentro de la sección \[terminal:TERM\] (donde TERM es el
nombre de nuestro terminal actual) o descartarlas. Si todas nuestras teclas funcionan
correctamente y no debemos corregir ninguna, entonces (lógico) no se grabará.

### Sistema de Archivos Virtual (VFS) <a id="virtual-fs"></a>

Este diálogo permite ajustar opciones del
[Sistema de Archivos Virtual (VFS)](#virtual-file-system).

M-Commander guarda en memoria o en disco información de algunos
de los sistemas de archivos virtuales con el fin de acelerar el acceso
a sus archivos.  Ejemplo de esto son los listados descargados desde
servidores FTP o los archivos temporales descomprimidos creados para
acceder rápidamente a los contenidos de archivos tipo tar comprimidos.

Esas informaciones se conservan para permitirnos navegar, salir y volver a
entrar rápidamente en los correspondientes sistemas de archivos virtuales.
Al cabo de un cierto tiempo sin usarlos deben ser liberados y recuperar
los recursos utilizados.  Por defecto ese tiempo es de un minuto, pero
se puede configurar por el usuario.

También podemos adelantar la liberación de los VFS desde el diálogo de
control de
*Directorios virtuales (VFS).*

El
[Sistema de Archivos FTP (FTPfs)](#ftp-file-system)
permite navegar por los directorios de servidores FTP remotos. Admite
diversas opciones.

*Contraseña de FTP anónimo*
es la contraseña a utilizar en conexiones en modo anónimo, esto es,
empleando el nombre de usuario "anonymous".  Algunos sitios exigen que
esta sea una dirección de correo electrónico válida, pero tampoco es
conveniente dar nuestra dirección real a desconocidos para protegernos
de los envíos de correo masivo.

FTPfs conserva en caché los listados de los directorios consultados.
La duración de la caché es el valor indicado tras
*Descartar el caché FTPfs.*
Un valor pequeño ralentiza el proceso porque cualquier pequeña operación
iría siempre acompañada de una conexión con el servidor FTP.

Se puede configurar un sistema proxy para FTP, aunque los cortafuegos
modernos son transparentes (al menos para FTP pasivo, ver más abajo)
y está opción es generalmente innecesaria.

Si la opción
*Usar siempre proxy*
no está activa, aún se puede emplear el proxy en casos concretos. Véanse
los ejemplos en la sección
[Sistema de Archivos FTP (FTPfs)](#ftp-file-system).

Si la opción
*Usar siempre proxy*
está puesta, el programa asume que cualquier nombre de máquina
sin puntos es accesible directamente y también consulta el archivo
{{sysconfdir}}/mcommander/mc.no_proxy en busca de nombres de máquinas locales (o
dominios completos si el nombre empieza con un punto).  En todos los
demás casos se usará siempre el proxy de FTP indicado arriba.

Se puede usar el archivo
*~/.netrc,*
que contiene información de usuarios y contraseñas para determinados
servidores FTP.  Para conocer el formato de los archivos .netrc véase la
página de manual sobre
**netrc (5)**.

*Usar FTP pasivo*
habilita el modo de tranferencia FTP pasivo (la conexión para
transferencia de datos es iniciada por la máquina cliente, no por el
servidor).  Esta opción es la recomendada, y de hecho está activada
por defecto.  Si se desactiva, la conexión la inicia el servidor,
y puede ser impedida por algún cortafuegos.

### Guardar Configuración <a id="save-setup"></a>

Al arrancar M-Commander se carga la información de inicio del
archivo
*~/.config/mc6/ini.*
Si este no existe, se cargará la información del
archivo de configuración genérico del sistema,
*{{pkgdatadir}}/mc.ini.*
Si el archivo de configuración genérico del sistema no existe, M-Commander utiliza
la configuración por defecto.

El comando
*Guardar Configuración*
crea el archivo
*~/.config/mc6/ini*
guardando la configuración actual de los menús
[Izquierdo, Derecho](#left-and-right-menus)
y
[Opciones](#options-menu).

Si se activa la opción
*Auto-guarda configuración,*
M-Commander guardará siempre la configuración actual al salir.

Existen también configuraciones que no pueden ser cambiadas desde
los menús.  Para cambiarlas hay que editar manualmente el archivo de
configuración.  Para más información, véase la sección sobre
[Ajustes Especiales](#special-settings).

<!-- help:break -->

# Ejecutando Órdenes del Sistema Operativo <a id="executing-operating-system-commands"></a>

Podemos ejecutar comandos tecleando en la línea de órdenes de
M-Commander, o seleccionando el programa que queremos ejecutar
en alguno de los paneles y pulsando
*Intro.*

Si pulsamos
*Intro*
sobre un archivo que no es ejecutable,
M-Commander compara la extensión del archivo seleccionado con las extensiones
recogidas en el
[Archivo de Extensiones](#edit-extension-file).
Si se produce una coincidencia se ejecutará el código asociado con esa extensión.
Tendrá lugar una
[expansión](#macro-substitution)
muy simple antes de ejecutar el comando.

## Comando cd Interno <a id="the-cd-internal-command"></a>

El comando
*cd*
es interpretado directamente por M-Commander, en vez de pasarlo
al interprete de comandos para su ejecución. Por ello puede que no todas
las posibilidades de expansión y sustitución de macro que hace nuestro
shell estén disponibles, pero sí algunas de ellas:

*Sustitución de tilde.*
La tilde (~) será sustituida por nuestro directorio de inicio. Si
añadimos un nombre de usuario tras la tilde, entonces será sustituido
por el directorio de entrada al sistema del usuario especificado.

Por ejemplo, ~coco sería el directorio de un supuesto usuario denominado
"coco", mientras que ~/coco es el directorio coco dentro de nuestro
propio directorio de inicio.

*Directorio anterior.*
Podemos volver al directorio donde estábamos anteriormente empleando el
nombre de directorio especial '-' del siguiente modo:
**cd -**

*Directorios en CDPATH.*
Si el directorio especificado al comando
**cd**
no está en el directorio actual, entonces M-Commander utiliza el
valor de la variable de entorno
**CDPATH**
para buscar el directorio en cualquiera de los directorios enumerados.

Por ejemplo, podríamos asignar a nuestra variable
**CDPATH**
el valor ~/src:/usr/src, lo que nos permitiría cambiar de directorio
a cualquiera de los directorios dentro de ~/src y /usr/src, desde
cualquier lugar del sistema de archivos, usando solo su nombre relativo
(por ejemplo cd linux podría llevarnos a /usr/src/linux).

## Sustitución de Macro <a id="macro-substitution"></a>

Cuando se accede al
[menú de usuario](#edit-menu-file),
o se ejecuta un
[comando dependiente de extensión](#edit-extension-file),
o se ejecuta un comando desde la línea de entrada de comandos,
se realiza una simple sustitución de macro.

Las macros son:

*%f*
: Archivo actual.

*%d*
: Nombre del directorio actual.

*%F*
: Archivo actual en el panel inactivo.

*%D*
: Directorio del panel inactivo.

*%t*
: Archivos actualmente marcados.

*%T*
: Archivos marcados en el panel inactivo.

*%u*
y
*%U*

> Similar a las macros %t y %T, salvo que los archivos quedan desmarcados.
> Solo se puede emplear esta macro una vez por cada entrada del archivo de menú
> o archivo de extensiones, puesto que para la siguiente vez no quedaría ningún
> archivo marcado.

*%s*
y
*%S*

> Archivos seleccionados: Los archivos marcados si los hay y si no el
> archivo actual.

*%cd*
: Esta es una macro especial usada para cambiar del directorio actual
al directorio especificado frente a él. Esto se utiliza principalmente
como interfaz con el
[Sistema de Archivos Virtual](#virtual-file-system).

*%view*
: Esta macro es usada para invocar al visor interno. Puede ser utilizada
en solitario, o bien con argumentos. Si pasamos algún argumento a esta
macro, deberá ser entre paréntesis.

> Los argumentos son:
> *ascii*
> para forzar al visor a modo ascii;
> *hex*
> para forzar al visor a modo hexadecimal;
> *nroff*
> para indicar al visor que debe interpretar las secuencias de negrita y
> subrayado de nroff;
> *unformated*
> para indicar al visor que no interprete los comandos nroff referentes a texto
> resaltado o subrayado.

*%%*
: El carácter %

*%{cualquier texto}*
: Pregunta sobre la sustitución. Un cuadro de entrada es mostrado y el texto dentro
de las llaves se usa como mensaje. La macro es sustituida por el texto
tecleado por el usuario. El usuario puede pulsar
*Esc* o *F10*
para cancelar. Esta macro no funciona aún sobre la línea de órdenes.

## Soporte de Subshell <a id="the-terminal"></a>

El soporte del subshell es una opción de tiempo de compilación, que funciona con los
shells: bash, tcsh y zsh.

Cuando el código del subshell es activado M-Commander
engendrará una copia de nuestro shell (la definida en la variable
**SHELL**
y si no está definida, el que aparece en el archivo /etc/passwd)
y lo ejecuta en un pseudoterminal, en lugar de invocar un nuevo shell
cada vez que ejecutamos un comando, el comando será pasado al
subshell como si lo hubiésemos escrito. Esto además permite cambiar las
variables de entorno, usaremos las funciones del shell y los alias definidos
que serán válidos hasta salir de M-Commander.

Si estamos usando
**bash**
podremos especificar comandos de arranque
para el subshell en nuestro archivo ~/.local/share/mc6/bashrc y
mapas de teclado especiales en el archivo ~/.local/share/mc6/inputrc.
Los usuarios de
**tcsh**
podrán especificar los comandos de arranque en el archivo ~/.local/share/mc6/tcshrc.

Cuando utilizamos el código del subshell, podemos suspender aplicaciones en
cualquier momento con la secuencia
*Ctrl-o*
y volver a M-Commander, si interrumpimos una aplicación, no
podremos ejecutar otros comandos externos hasta que quitemos la
aplicación que hemos interrumpido.

Una característica extra añadida de uso del subshell es que el prompt
mostrado por M-Commander es el mismo que estamos usando en
nuestro shell.

La sección
[OPCIONES](#options)
tiene más información sobre cómo controlar el código del subshell.

# Cambiar Permisos <a id="chmod"></a>

Cambiar Permisos se usa para cambiar los bits de permisos en un grupo de
archivos y directorios. Puede ser invocado con la combinación de teclas Ctrl-x c.

La ventana de Cambiar Permisos tiene dos partes -
*Permisos*
y
*Archivo*

En la sección Archivo se muestran el nombre del archivo o directorio
y sus permisos en formato numérico octal, así como su propietario y grupo.

En la sección de Permisos hay un grupo de casillas de selección
que corresponden a los posibles permisos del archivo. Conforme los cambiamos
podemos ver cómo el valor octal va cambiando en la sección Archivo.

Para desplazarse entre las casillas y botones de la ventana podemos
usar las
*teclas del cursor*
o la
*tecla de tabulación.*
Para marcar o desmarcar casillas y para pulsar los botones
usaremos la
*barra espaciadora.*
Podemos usar los atajos de teclado (las letras destacadas) para accionar
directamente los elementos.

Para aceptar y aplicar los permisos, usaremos la tecla
*Intro.*

Si se trata de un grupo de archivos o directorios, podemos cambiar parte
de los permisos marcándolos (las marcas son los asteriscos a la izquierda de las
casillas) y pulsando el botón
**[\* Poner]**
o
**[\* Quitar]**
para indicar la acción deseada. Los permisos no marcados conservan, en este
caso, los valores previos.

Podemos también fijar todos los permisos iguales en todos los archivos
con el botón
**[Todos]**
o solo los permisos marcados con el botón
**[\* Todos].**
En estos casos las casillas indican el estado en que queda cada permiso, igual
que para archivos individuales.

**[Todos]**
actúa sobre todos los permisos de todos los archivos

**[\* Todos]**
actúa solo sobre los atributos marcados de los archivos

**[\* Poner]**
activa los permisos marcados en los archivos seleccionados

**[\* Quitar]**
desactiva los permisos marcados en los archivos seleccionados

**[Aplicar]**
actúa sobre todos los permisos de cada archivo, uno a uno

**[Cancelar]**
cancela Cambiar Permisos

# Cambiar Dueño <a id="chown"></a>

Cambiar Dueño permite cambiar el propietario y/o grupo de un archivo. La tecla
rápida para este comando es Ctrl-x o.

# Cambiar Dueño y Permisos <a id="advanced-chown"></a>

Cambiar Dueño y Permisos combina
[Cambiar Dueño](#chown)
y
[Cambiar Permisos](#chmod)
en una única ventana. Se puede así cambiar los permisos, propietario y grupo
del archivo de una sola vez.

# Operaciones con Archivos <a id="file-operations"></a>

Cuando copiamos, movemos o borramos archivos, M-Commander muestra el
diálogo de operaciones con archivos. En él aparecen los archivos que se estén procesando
y hasta tres barras de progreso. La barra de archivo indica qué parte del archivo actual
va siendo copiada, la barra de contador indica cuántos de los archivos marcados
han sido completados y la barra de bytes nos dice qué parte del tamaño total de archivos
marcados ha sido procesado hasta el momento. Si la operación detallada está desactivada
no se muestran las barras de archivo y bytes.

En la parte inferior hay dos botones. Pulsando el botón Saltar se
ignorará el resto del archivo actual. Pulsando el botón
Abortar se detendrá la operación y se ignora el resto de archivos.

Hay otros tres diálogos que pueden aparecer durante operaciones de
archivos.

El diálogo de error informa sobre una condición de error y tiene tres
posibilidades. Normalmente seleccionaremos el botón Saltar para evitar el archivo
o Abortar para detener la operación. También podemos seleccionar el botón
Reintentar si hemos corregido el problema desde otro terminal.

El diálogo Reemplazar aparece cuando intentamos copiar o mover un archivo
sobre otro ya existente. El mensaje muestra fechas y tamaños de ambos archivos.
Pulsaremos el botón Sí para sobreescribir el archivo, el botón No
para saltarlo, el botón Todos para sobreescribir todos los archivos,
Ninguno para no sobreescribir en ningún caso y Actualizar para sobreescribir
si el archivo origen es posterior al archivo objeto. Podemos abortar toda la
operación pulsando el botón Abortar.

El diálogo de eliminación recursiva aparece cuando intentamos borrar
un directorio no vacío. Pulsaremos Sí para borrar el directorio recursivamente,
No para saltar el directorio, Todo para borrar recursivamente todos los directorios
marcados no vacíos y Ninguno para saltarlos todos. Podemos abortar toda la
operación pulsando el botón Abortar. Si seleccionamos el botón Sí o Todo
se nos pedirá confirmación. Diremos "sí" solo si estamos realmente seguros
de que queremos una eliminación recursiva.

Si hemos marcado archivos y realizamos una operación sobre ellos, solo
los archivos sobre los que la operación fue exitosa son desmarcados. Los archivos
saltados y aquellos en los que la operación falló permanecen marcados.

# Copiar/Renombrar con Máscara <a id="mask-copyrename"></a>

Las operaciones de copiar/mover permiten transformar los nombres de los archivos
de manera sencilla. Para ello, hay que procurar una máscara correcta para el
origen y normalmente en la terminación del destino algunos caracteres comodín.
Todos los archivos que concuerden con la máscara origen son copiados/renombrados
según la máscara destino. Si hay archivos marcados, solo aquellos que encajen con
la máscara de origen serán renombrados.

Hay otras opción que podemos seleccionar:

Seguir Enlaces indica si los enlaces simbólicos o físicos en el directorio
origen (y recursivamente en sus subdirectorios) producen nuevos enlaces en el
directorio destino o si queremos copiar su contenido.

Copiar Recursivamente indica qué hacer si en el directorio
destino existe ya un directorio con el mismo nombre que el
archivo/directorio que está siendo copiado. La acción por defecto
es copiar su contenido sobre ese directorio. Habilitando esto
podemos copiar el directorio de origen dentro de ese directorio.
Quizás un ejemplo pueda ayudar:

Queremos copiar el contenido de un directorio denominado coco a /blas
donde ya existe un directorio /blas/coco. Por defecto, mcommander copiaría el
contenido en /blas/coco, pero con esta opción se copiaría como
/blas/coco/coco.

Preservar Atributos indica que se deben conservar los permisos originales
de los archivos, marcas temporales y si somos superusuario también el
propietario y grupo originales.
Si esta opción no está activa se aplica el valor actual de umask.

**Usando Patrones Shell activado**

Usando Patrones Shell nos permite usar los caracteres comodín '\*' y '?'
en la máscara de origen. Funcionará igual que en la línea de órdenes. En
la máscara destino, solo están permitidos los comodines '\*' y '\\\<número>'.
El primer '\*' en la máscara destino corresponde al primer grupo del comodín
en la máscara de origen, el segundo '\*' al segundo grupo, etcétera.
El comodín '\\1' corresponde al primer grupo en la máscara de origen,
el comodín '\\2' al segundo y así sucesivamente hasta '\\9'. El comodín '\\0'
es el nombre completo del archivo fuente.

Dos ejemplos:

Si la máscara de origen es "\*.tar.gz", el destino es "/blas/\*.tgz" y el
archivo a copiar es "coco.tar.gz", la copia se hará como "coco.tgz"
en "/blas".

Supongamos que queremos intercambiar el nombre y la extensión de modo que
"archivo.c" se convierta en "c.archivo". La máscara origen será "\*.\*" y
la de destino "\\2.\\1".

**Usando Patrones Shell desactivado**

Cuando la opción de Patrones Shell está desactivada M-Commander no realiza una
agrupación automática. Deberemos usar expresiones '\\(...\\)' en la máscara
origen para especificar el significado de los comodines en la máscara destino.
Esto es más flexible pero también necesita más escritura. Por lo demás,
las máscaras destino son similares al caso de Patrones Shell activos.

Dos ejemplos:

Si la máscara de origen es "^\\(.\*\\)\\.tar\\.gz$", el destino es
"/blas/\*.tgz" y el archivo a ser copiado es "coco.tar.gz", la copia
será "/blas/coco.tgz".

Si queremos intercambiar el nombre y la extensión para que "archivo.c"
sea "c.archivo", la máscara de origen puede ser
"^\\(.\*\\)\\.\\(.\*\\)$" y la de destino "\\2.\\1".

**Capitalización**

Podemos hacer cambios entre mayúsculas y minúsculas en los nombres de archivos.
Si usamos '\\u' o '\\l' en la máscara destino, el siguiente carácter será convertido a
mayúsculas o minúsculas respectivamente.

Si usamos '\\U' o '\\L' en la máscara destino, los siguientes caracteres
serán convertidos a mayúsculas o minúsculas respectivamente hasta encontrar
'\\E' o un segundo '\\U' o '\\L' o el fin del nombre del archivo.

'\\u' y '\\l' tienen prioridad sobre '\\U' y '\\L'.

Por ejemplo, si la máscara fuente es '\*' (con Patrones Shell activo) o '^\\(.\*\\)$'
(Patrones Shell desactivado) y la máscara destino es '\\L\\u\*' los nombres de archivos
serán convertidos para que tengan su inicial en mayúscula y el resto del nombre en
minúsculas.

También podemos usar '\\' como carácter de escape evitando la interpretación de todos
estos caracteres especiales. Por ejemplo, '\\\\' es
una contrabarra y '\\\*' es un asterisco.

# Seleccionar/Deseleccionar Archivos <a id="selectunselect-files"></a>

El diálogo permite seleccionar o deseleccionar grupos de archivos y
directorios. La
[línea de entrada](#input-line-keys)
permite introducir una expresión regular para los nombres de los
archivos a seleccionar/deseleccionar.

Indicando
*Solo archivos*
los directorios no se seleccionan.  Con los
*Caracteres Comodín*
habilitados, se pueden introducir expresiones regulares del tipo empleado en
los patrones de nombres de la shell (poniendo \* para cero o más caracteres y ?
para uno o más caracteres).  Si los
*Caracteres Comodín*
están deshabilitados, entonces la selección de archivos se realiza con expresiones
regulares normales.  Véase la página de manual de
**ed (1)**.
Finalmente, si no se activa
*Distinguir May/min*
la selección se hará sin distinguir caracteres en mayúsculas o minúsculas.

# Comparador de Archivos Interno <a id="diff-viewer"></a>

El comparador de archivos interno permite comparar dos archivos y editarlos
en el sitio quedando la comparación actualizada sobre la marcha.  Se puede
navegar y ver copias de trabajos desde los sistemas de control de versiones
populares (GIT, Subversion, etc).

El comparador ofrece los siguientes atajos de teclado:

**F1**
: Invoca el visor de ayuda y muestra esta sección.

**F2**
: Guarda los archivos modificados.

**F4**
: Edita el archivo del panel izquierdo.

**F14**
: Edita el archivo del panel derecho.

**F5**
: Combina el fragmento actual. Solo se combina el fragmento actual.

**F7**
: Comenzar una búsqueda.

**F17**
: Repetir la búsqueda previa.

**F10, Esc, q**
: Salir del comparador.

**Alt-s, s**
: Mostrar/ocultar el estado de los fragmentos.

**Alt-n, l**
: Mostrar/ocultar números de línea.

**f**
: Maximizar el panel izquierdo.

**=**
: Igualar el ancho de los paneles.

**>**
: Reducir el panel derecho.

**<**
: Reducir el panel izquierdo.

**c**
: Mostrar/ocultar «^M» para los saltos de línea con carácter de retorno (CR).

**2, 3, 4, 8**
: Fijar ancho de tabulaciones.

**Ctrl-u**
: Intercambia el contenido de los paneles.

**Ctrl-r**
: Actualizar la pantalla.

**Ctrl-o**
: Alternar con la pantalla de órdenes del sistema.

**Intro, Espacio, n**
: Avanzar al siguiente fragmento diferente.

**Backspace, p**
: Retroceder al fragmento diferente anterior.

**g**
: Saltar a la línea indicada.

**Abajo**
: Avanzar una línea.

**Ariba**
: Retroceder una línea.

**AvPág (Página adelante)**
: Avanza una página hacia abajo.

**RePág (Página atrás)**
: Retrocede una página hacia arriba.

**Inicio, A1**
: Va al comienzo de la línea.

**Fin**
: Va al final de la línea.

**Ctrl-Inicio**
: Vuelve al comienzo del archivo.

**Ctrl-Fin, C1**
: Avanza hasta el final del archivo.

# Visor de Archivos Interno <a id="internal-file-viewer"></a>

El visor de archivos interno ofrece dos modos de presentación: ASCII y
hexadecimal. Para alternar entre ambos modos, se emplea la tecla F4.

El visor intenta usar el mejor método disponible en el sistema, según
el tipo de archivo, para mostrar información. Los archivos comprimidos
se descomprimen automáticamente si los programas correspondientes (GNU
gzip ó bzip2) están instalados en el sistema. El propio visor es capaz
de interpretar ciertas secuencias de caracteres que se emplean para
activar los atributos de negrita y subrayado, mejorando la presentación
de los archivos.

En modo hexadecimal, la función de búsqueda admite texto entre comillas o
valores numéricos. El texto entrecomillado se busca tal cual (retirando
las comillas) y cada número se corresponde a un byte. Unos y otros se
pueden entremezclar como en:

```
"Cadena" -1 0xBB 012 "otro texto"
```

Nótese que 012 es un número octal y -1 se convierte en 0xFF.

He aquí una lista de las acciones asociadas a cada tecla que
M-Commander gestiona en el visor interno de archivos.

**F1**
Invoca el visor de ayuda de hipertexto interno.

**F2**
Cambia el modo de ajuste de líneas en pantalla.

**\* N. del T.:**
: Envuelta (Ajustada), se muestra toda la información de la línea en la pantalla, de
modo que si esta ocupa más del ancho de la pantalla aparece como si fuese
otra línea aparte o bien desenvuelta (desajustada), truncando el contenido
de la línea que sobresale de la pantalla. Este contenido puede ser
consultado utilizando las teclas del cursor.

**F4**
Cambia entre el modo hexadecimal y el Ascii.

**F5**
Ir a la línea. Nos pedirá el número de línea en el que deseamos posicionarnos y
mostrará el archivo a partir de esa línea.

**F6, /.**
Búsqueda de expresión regular desde la posición actual hacia adelante.

**?,**
Búsqueda de expresión regular desde la posición actual hacia atrás.

**F7**
Búsqueda normal/ búsqueda en modo hexadecimal.

**Ctrl-s.**
Comienza una búsqueda normal si no existe una expresión de búsqueda previa si no
busca la próxima coincidencia.

**Ctrl-r.**
Comienza una búsqueda hacia atrás si no había expresión de búsqueda anterior si no
busca la próxima coincidencia.

**n.**
Buscar la próxima coincidencia.

**F8**
Intercambia entre el modo crudo y procesado: esto mostrará el archivo como se encuentra en disco
o si se ha especificado un filtro de visualización en el archivo extensions.ini, entonces
la salida filtrada. El modo actual es siempre el contrario al mostrado
en la etiqueta del botón, en tanto que el botón muestra el modo en el que entraremos
con la pulsación de esa tecla.

**F9**
Alterna entre la visualización con y sin formato: en el modo con formato
se interpretan algunas secuencias de caracteres para mostrar texto en
negrita y subrayado con diferentes colores. Como en el caso anterior,
la etiqueta del botón muestra el estado contrario al actual.

**F10, Esc.**
Sale del visor interno.

**AvPág, espacio, Ctrl-v.**
Avanza una página hacia abajo.

**RePág, Alt-v, Ctrl-b, Borrar.**
Retrocede una página hacia arriba.

**Cursor Abajo**
Desplaza el texto una línea hacia arriba, mostrando en la línea inferior de
la pantalla una nueva línea que antes quedaba oculta.

**Cursor Arriba**
Desplaza una línea hacia abajo.

**Ctrl-l**
Redibuja el contenido de la pantalla.

**!**
Engendra un nuevo shell en el directorio de trabajo actual.

**[n] m**
Coloca la marca n.

**[n] r**
Salta hasta la marca n.

**Ctrl-f**
Salta al archivo siguiente.

**Ctrl-b**
Ídem al archivo anterior.

**Alt-r**
Intercambia entre los diferentes modos de regla: desactivado, arriba, abajo.

Es posible adiestrar al visor de archivos sobre cómo mostrar un archivo, mírese
la sección
[Editar Archivo de Extensiones](#edit-extension-file).

# Editor de Archivos Interno <a id="internal-file-editor"></a>

El editor de archivos interno es un editor a pantalla completa de
avanzadas prestaciones. Puede editar archivos de hasta 64 MB y también
permite modificar archivos binarios. Se inicia pulsando
**F4**
supuesto que la variable
*use_internal_edit*
esté presente en el archivo de inicialización.

Las características soportadas actualmente son: copia, desplazamiento,
borrado, corte, y pegado de bloques; deshacer paso a paso; menús
desplegables; inserción de archivos; definición de macros; buscar y
reemplazar usando expresiones regulares); selección de texto con
mayúsculas-cursor (si el terminal lo soporta); alternancia
insertar-sobreescribir; plegado de líneas; sangrado automático; tamaño
de tabulación configurable; realce de sintaxis para varios tipos de
archivos; y la opción de pasar bloques de texto por filtros externos
como indent o ispell.

El editor es muy fácil de usar y no requiere aprendizaje alguno.
Para conocer las teclas asignadas a cada función, basta consultar los
menús correspondientes. Además, las teclas de desplazamiento con la
tecla de mayúsculas seleccionan texto. Se puede seleccionar con el ratón,
aunque podemos recuperar su funcionamiento habitual en terminales (copiar
y pegar) manteniendo pulsada la tecla mayúsculas.
**Ctrl-Ins**
copia al archivo
**mcedit6.clip**
y
**Mayús-Ins**
pega desde
**mcedit6.clip**.
**Mayús-Supr**
corta y copia en
**mcedit6.clip**,
y
**Ctrl-Supr**
elimina el texto resaltado. La tecla
**Intro**
produce un salto de línea con sangrado automático opcional.

Para definir una macro, pulsar
**Ctrl-r**
y entonces teclearemos las secuencias de teclas que deseamos sean
ejecutadas. Pulsaremos
**Ctrl-r**
de nuevo al finalizar. Podemos asignar la macro a la tecla que queramos
pulsando sobre ella. La macro será ejecutada cuando pulsemos
**Ctrl-a**
seguido de la tecla asignada. También será ejecutada si
pulsamos Meta (Alt), Ctrl, o Escape y la tecla asignada, siempre y cuando
la tecla no sea usada por ninguna otra función. Una vez definida, los comandos de
macro irán al archivo
**~/.local/share/mc6/mcedit6/mcedit6.macros**
en nuestro directorio de inicio. Podemos eliminar una macro borrando
la línea adecuada en este archivo.

**F19**
formateará el bloque seleccionado (sea texto, código
**C**
o
**C++**
u otro). Esto está controlado por el archivo
**{{pkgdatadir}}/edit.indent.rc**
que se copia la primera vez que se usa en
**~/.local/share/mc6/mcedit6/edit.indent.rc**
en el directorio personal.

El editor también visualiza caracteres no estadounidenses (160+). Al editar
archivos binarios, debemos configurar los
**bits de pantalla**
a 7 bits en el menú de opciones para mantener el espaciado saneado.

# Terminación <a id="completion"></a>

Permite a M-Commander escribir por nosotros.

Intenta completar el texto escrito antes de la posición
actual.  M-Commander intenta la terminación tratando
el texto como si fuera una variable (si el texto comienza con
**$**),
nombre de usuario (si el texto empieza por
**~**),
nombre de máquina (si el texto comienza con
**@**)
o un comando (si estamos en la línea de órdenes en una posición
donde podríamos escribir un comando; las terminaciones posibles entonces
incluyen las palabras reservadas del shell así como comandos internos
del shell) en ese orden. Si nada de lo anterior es aplicable, se intenta
la terminación con nombres de archivo.

La terminación de nombres de archivo, usuario y máquina funciona en
todas las líneas de entrada; la terminación de comandos es específica de
la línea de órdenes. Si la terminación es ambigua (hay varias
posibilidades diferentes), M-Commander pita, y la acción siguiente
depende de la opción
*Completar: Mostrar Todos*
en el diálogo de
[Configuración](#configuration).
Si está activada, se despliega inmediatamente junto a la posición actual
una lista con todas las posibilidades donde se puede seleccionar con
las flechas de movimiento e
**Intro**
la entrada correcta. También podemos seguir escribiendo caracteres con lo
que la línea se sigue completando tanto como sea posible y simultáneamente
la primera entrada coincidente de la lista se va resaltando. Si volvemos
a pulsar
**Alt-Tab**,
solo las coincidencias permanecen en la lista. Tan pronto
como no haya ambigüedad, la lista desaparece; también podemos quitarla
con las teclas de cancelación
**Esc**, **F10**
y las teclas de movimiento a izquierda y derecha. Si
[Completar: Mostrar Todos](#configuration)
está desactivado, la lista aparece cuando pulsamos
**Alt-Tab**
por segunda vez; con la primera M-Commander solo emite un pitido.

Aplica escapes a los símbolos
**?**, **\*** y **&**
(como **\\?**, **\\\***, **\\&** )
en los nombres de archivo para evitar su interpretación en expresiones
regulares al realizar sustituciones en la línea de entrada.

# Sistemas de Archivos Virtuales (VFS) <a id="virtual-file-system"></a>

M-Commander dispone de una capa de código de acceso al sistema
de archivos; esta capa se denomina Sistema de Archivos Virtual (VFS).
El Sistema de Archivos Virtual permite a M-Commander manipular
archivos no ubicados en el sistema de archivos Unix.

M-Commander incluye actualmente varios Sistemas de Archivos
Virtuales: el sistema de archivos
*local,*
utilizado para acceder al sistema de archivos Unix habitual;
*undelfs*
para recuperar archivos borrados en sistemas de archivos de tipo ext2
(sistema de archivos habitual en Linux);
*ftpfs*
para manipular archivos en sistemas remotos a través de FTP;
*fish*
para manipular archivos a través de conexiones a shell como rsh o ssh.

Dependiendo de la forma en que fue compilado, puede disponer también de:
*sftpfs*
para manipular archivos en sistemas remotos a través de SFTP.

Se facilita también un sistema de archivos genérico
*extfs*
(EXternal virtual File System) para extender con facilidad las
posibilidades de VFS empleando guiones y programas externos.

El código VFS interpretará todos los nombres de ruta usados y los dirigirá
al sistema de archivos correcto. El formato usado para cada uno de los
sistemas de archivos se describe más adelante en su propia sección.

## Sistema de archivos FTP <a id="ftp-file-system"></a>

FTPfs permite manipular archivos en máquinas remotas. Para utilizarlo
se puede emplear la opción de menú
*Conexión por FTP*
o simplemente emplear la orden
*cd*
como cuando cambiamos habitualmente de directorio, pero indicando
como ruta:

*ftp://[!][usuario[:clave]@]maquina[:puerto]/[dir-remoto]*

Los elementos
*usuario,*
*puerto*
y
*directorio-remoto*
son opcionales. Si especificamos el elemento
*usuario,*
entonces M-Commander intentará conectarse con la máquina remota
como ese usuario, y si no, establecerá una conexión en modo anónimo o
con el nombre de usuario indicado en el archivo
*~/.netrc.*
El elemento
*clave*
también es opcional, y si está presente, se emplea como contraseña
de acceso.  Esta forma de colocar la contraseña como parte del nombre
del directorio virtual no es muy recomendable porque eventualmente puede
aparecer en pantalla y guardarse en el histórico de directorios.

Si es necesario utilizar un proxy de FTP, se añade un símbolo de exclamación
**!**
delante del nombre de la máquina.

Ejemplos:

```
    ftp://ftp.nuclecu.unam.mx/linux/local
    ftp://tsx-11.mit.edu/pub/linux/packages
    ftp://!detras.barrera.edu/pub
    ftp://guest@pcremoto.com:40/pub
    ftp://miguel:xxx@servidor/pub
    ftp://ftp.um.es/pub
```

La opciones de FTPfs se encuentran entre las opciones de configuración del
[Sistema de Archivos Virtual (VFS)](#virtual-fs).

## Sistema de archivos a través de SHell <a id="file-transfer-over-shell-filesystem"></a>

El FISH es un sistema de archivos por red que permite manipular archivos
en una máquina remota como si estuvieran almacenados localmente.  Para
ello es preciso que el sistema remoto esté ejecutando el servidor FISH
o permitir la conexión a una shell de tipo bash.

Para conectar con la máquina remota basta cambiar de directorio a un
directorio virtual cuyo nombre sea de la forma:

*sh://[usuario@]maquina[:opciones]/[directorio-remoto]*

Los elementos
*usuario,*
*opciones*
y
*directorio-remoto*
son opcionales. Si se especifica el elemento
*usuario*
M-Commander intentará entrar en la máquina remota como ese
usuario, y si no usará nuestro nombre.

Como
*opciones*
se puede poner 'C' para usar compresión y 'r' para utilizar una
conexión rsh en vez de ssh. Si se indica el
*directorio-remoto,*
se buscará este como primer directorio al conectar con la máquina
remota.

Ejemplos:

```
    sh://solorsh.es:r/linux/local
    sh://pepe@quiero.comprension.edu:C/privado
    sh://pepe@sincomprimir.ssh.edu/privado
```

## Sistema de archivos SFTP (FTP sobre SSH) <a id="sftp-ssh-file-transfer-protocol-filesystem"></a>

El sistema de archivos SFTP es un sistema de archivos sobre red
que permite manipular archivos en una máquina remota como si fueran
locales.

Para conectar con la máquina remota basta cambiar de directorio a un
directorio virtual cuyo nombre sea de la forma:

*sftp://[usuario@]maquina:[puerto]/[directorio-remoto]*

Los elementos
*usuario,*
*puerto*
y
*directorio-remoto*
son opcionales. Si se especifica el elemento
*usuario*
M-Commander intentará acceder a la máquina remota como ese
usuario, y si no usará nuestro nombre. El
*puerto*
indica el puerto utilizado por el servidor remoto, por defecto 22.
El
*directorio-remoto*
será el directorio actual tras la conexión.

Ejemplos:

```
    sftp://solorsh.es/linux/local
    sftp://pepe:k1abe@quiero.comprension.edu/privado
    sftp://pepe@sincomprimir.ssh.edu/privado
    sftp://pepe@maquina.ssh.edu:2222/privado
```

## Sistema de archivos EXTerno (extfs) <a id="external-file-system"></a>

**extfs**
permite incorporar a M-Commander numerosas utilidades y tipos
de archivos de manera sencilla, simplemente escribiendo guiones
(scripts).

Los sistemas de archivos Extfs son de dos tipos:

1\. Sistemas de archivos autónomos, que no están asociados a ningún
archivo existente. Representan algún tipo de información relacionada con
el sistema en forma de árbol de directorios. Se accede a ellos ejecutando
*'cd nombrefs://'*
donde nombrefs es el nombre corto que identifica el extfs (ver más
adelante).  Ejemplos de estos son audio (lista de pistas de sonido en
el CD) o apt (lista de paquetes de tipo Debian en el sistema).

Por ejemplo, para listar las pistas de música del CD, escribir

```
  cd audio://
```

2\. Sistemas de archivos en un archivo (como rpm, patchfs y más), que
muestran los contenidos de un archivo en forma de árbol de directorios.
Puede tratarse de archivos reales empaquetados o comprimidos en un archivo
(urar, rpm) o archivos virtuales, como puede ser el caso de mensajes
en un archivo de correo electrónico (mailfs) o partes de un archivo de
modificaciones o parches (patchfs). Para acceder a ellos se añade
*'nombrefs://'*
al nombre del archivo a abrir. Este archivo podría él mismo estar en
otro sistema de archivos virtual.

Por ejemplo, para listar los contenidos de un archivo documentos.zip
comprimido hay que escribir

```
  cd documentos.zip/uzip://
```

En muchos aspectos, se puede tratar un sistema de archivos externo como
cualquier otro directorio. Podríamos añadirlo a la lista de favoritos o
cambiar a él desde la historia de directorios. Una limitación importante
es que, estando dentro de él, no se puede ejecutar órdenes del sistema,
como por otra parte ocurre en cualquier sistema de archivos VFS no local.

M-Commander incluye inicialmente guiones para algunos sistemas de
archivos externos:

**a**
: acceder a un disquete DOS/Windows 'A:'
*(cd a://).*

**apt**
: monitor del sistema de gestión de paquetes APT de Debian
*(cd apt://).*

**audio**
: acceso y audición de CDs
*(cd audio://*
o
*cd dispositivo/audio://).*

**deb**
: paquete de la distribución GNU/Linux Debian
*(cd archivo.deb/deb://).*

**dpkg**
: paquetes instalados en Debian GNU/Linux
*(cd deb://).*

**hp48**
: ver o copiar archivos a/desde una calculadora HP48
*(cd hp48://).*

**lslR**
: navegación en listados lslR empleados en bastantes sitios FTP
*(cd filename/lslR://).*

**mailfs**
: soporte para archivos de correo electrónico tipo mbox
*(cd archivo_mbox/mailfs://).*

**patchfs**
: manipulación de archivos de cambios/parches tipo diff
*(cd archivo/patchfs://).*

**rpm**
: paquete RPM
*(cd archivo/rpm://).*

**rpms**
: base de datos de paquetes RPM instalados
*(cd rpms://).*

**ulha, urar, uzip, uzoo, uar, uha**
: herramientas de compresión
*(cd archivo/xxxx://*
siendo xxxx uno de estos:
*ulha,*
*urar,*
*uzip,*
*uzoo,*
*uar,*
*uha).*

Se pueden asociar extensiones o tipos de archivo a un determinado sistema
de archivos externo tal como se describe en la sección sobre cómo
[Editar el Archivo de Extensiones](#edit-extension-file)
de M-Commander. He aquí, a modo de ejemplo, una entrada para
paquetes Debian:

```
  regex/\.deb$
          Open=%cd %p/deb://
```

# Colores <a id="colors"></a>

M-Commander intentará determinar si nuestro terminal soporta
el uso de color utilizando la base de datos de terminales y nuestro nombre de terminal. Algunas veces
estará confundido, por lo que deberemos forzar el modo en color o deshabilitar el modo de color
usando el argumento -c y -b respectivamente.

Si el programa está compilado con el gestor pantallas S-Lang
en lugar de ncurses, también chequeará la variable
**COLORTERM**,
si existe, lo que tiene el mismo efecto que la opción -c.

Podemos especificar a los terminales que siempre fuercen el modo en color
añadiendo la variable
*color_terminals*
a la sección Colors del archivo de inicialización. Esto evitará que
M-Commander intente la detección de soporte de color. Ejemplo:

```
[Colors]
color_terminals=linux,xterm
```

```
color_terminals=nombre-terminal1,nombre-terminal2...
```

El programa puede compilarse con ncurses y S-Lang, ncurses no
ofrece la posibilidad de forzar el modo en color: ncurses utiliza la
información de la base de datos de terminales.

# Skins

Con los «skins» (pieles, caretas) se puede cambiar la apariencia global de
M-Commander.  Para ello hay que proporcionar un archivo que contenga
descripciones de colores y formas de trazar las líneas de borde de los
paneles y diálogos.  La redefinición de colores es completamente compatible
con la configuración tradicional detallada en la sección sobre
[Colores](#colors).

El archivo se busca, en orden, de varias maneras:  
: 1) La opción
**-S \<skin>**
o
**--skin=\<skin>**
al ejecutar mc.  
2) La variable de entorno
**MC_SKIN**.  
3) El parámetro
**skin**
en la sección
**[Midnight-Commander]**
del archivo de configuración.  
4) El archivo
**{{sysconfdir}}/mcommander/skins/default.ini**.  
5) El archivo
**{{pkgdatadir}}/skins/default.ini**.

En línea de órdenes, en la variable de entorno o el parámetro de la
configuración pueden contener la ruta absoluta al archivo de skin con
o sin su extensión .ini. De no indicar la ruta se realiza la búsqueda,
en orden, en:

> 1)
> **~/.local/share/mc6/skins/**.  
> 2)
> **{{sysconfdir}}/mcommander/skins/**.  
> 3)
> **{{pkgdatadir}}/skins/**.  

Para más información consultar:
: [Descripción de secciones y parámetros](#skins-sections)  
[Definiciones de pares de colores](#skins-colors)  
[Trazado de líneas](#skins-lines)  
[Compatibilidad](#skins-oldcolors)  

## Descripción de secciones y parámetros <a id="skins-sections"></a>

La sección
**[skin]**
contiene metadatos del archivo. El parámetro
*description*
proporciona un pequeño texto descriptivo.

La sección
**[filehighlight]**
contiene descripciones de pares de colores para el resaltado de nombres
de archivo.  Los nombres de parámetros de esta sección tienen que coincidir
con los nombres de sección del archivo
*filehighlight.ini.*

Para más información, véase la sección sobre
[Resaltado de nombres](#filenames-highlight).

La sección
**[core]**
permite definir elementos que se utilizan en otras partes.

*\_default\_*
: Colores por defecto.  Se utilizará en todas las secciones que
no contengan definición de colores.

*selected*
: cursor.

*marked*
: elementos seleccionados.

*markselect*
: cursor sobre elementos seleccionados.

*gauge*
: color de la parte completada en las barras de progreso.

*input*
: color de los recuadros de texto editable en los dialogos.

*inputmark*
: color de los textos editables en los dialogos.

*inputunchanged*
: color original de los textos editables antes de tocarlos.

*commandlinemark*
: color del texto seleccionado en la línea de órdenes.

*reverse*
: color inverso.

La sección
**[dialog]**
define elementos de las ventanas de diálogo salvo los diálogos de error.

*\_default\_*
: Colores por defecto para esta sección.  Se utilizará [core].\_default\_
si no se especifica

*dfocus*
: Color del elemento activo, con el foco.

*dhotnormal*
: Color de las teclas de acceso rápido.

*dhotfocus*
: Color de las teclas de acceso rápido del elemento activo.

La sección
**[error]**
define elementos de las ventanas de diálogo de error.

*\_default\_*
: Colores por defecto para esta sección.  Se utilizará [core].\_default\_
si no se especifica.

*errdhotnormal*
: Color de las teclas de acceso rápido.

*errdhotfocus*
: Color de las teclas de acceso rápido del elemento activo.

La sección
**[menu]**
define elementos de menú.  Esta sección afecta al menú general (activado
con F9) y a los menús de usuario (activados con F2 en la pantalla general
y con F11 en el editor).

*\_default\_*
: Colores por defecto para esta sección. Se utilizará [core].\_default\_
si no se especifica

*entry*
: Color de las entradas de menú.

*menuhot*
: Color de las teclas de acceso rápido en menú.

*menusel*
: Color de la entrada de menú activa, con el foco.

*menuhotsel*
: Color de las teclas de acceso rápido en la entrada activa de menú.

*menuinactive*
: Color de menú inactiva.

La sección
**[help]**
define los elementos de la ventana de ayuda.

*\_default\_*
: Colores por defecto para esta sección. Se utilizará [core].\_default\_
si no se especifica.

*helpitalic*
: Par de color para elementos en
**cursiva**.

*helpbold*
: Par de color para elementos
**resaltados**.

*helplink*
: Color de los enlaces

*helpslink*
: Color del enlace activo, con el foco.

La sección
**[editor]**
define los colores de los elementos que se encuentran en el editor.

*\_default\_*
: Colores por defecto para esta sección. Se utilizará [core].\_default\_
si no se especifica.

*editbold*
: Par de color para elementos
**resaltados**.

*editmarked*
: Color del texto seleccionado.

*editwhitespace*
: Color de las tabulaciones y espacios al final de línea resaltados.

*editlinestate*
: Color de la línea de estado.

La sección
**[viewer]**
define los colores de los elementos que se encuentran en el visor.

*viewunderline*
: Par de color para elementos
**subrayados**.

## Definiciones de pares de colores <a id="skins-colors"></a>

Cualquier parámetro del archivo de skin puede contener definiciones de
pares de color.

Un par de colores está formado por el nombre de los dos colores separados
por ';'. El primer color establece el color de frente y el segundo el
color de fondo. Se puede omitir alguno de los dos colores, en cuyo caso
se utilizará el color del par de color por defecto (par de color general
o del par de color por defecto en la sección).

Ejemplo:  

```
[core]
    # verde sobre negro
    _default_=green;black
    # verde (por defecto) sobre azul
    selected=;blue
    # amarillo sobre negro (por defecto)
    marked=yellow;
```

Los nombres de colores permitidos son los que aparecen en la sección
[Colores](#colors).

## Trazado de líneas <a id="skins-lines"></a>

Trazos de líneas de la sección
**[lines]**
del archivo de skins.  Por defecto se utilizan líneas sencillas, pero
se pueden redefinir empleando cualquier símbolo utf-8 (por ejemplo,
líneas dobles).

Descripción de parámetros de la sección
**[lines]**:

*lefttop*
: esquina superior izquierda.

*righttop*
: esquina superior derecha.

*centertop*
: unión central en el borde superior.

*centerbottom*
: unión central en el borde inferior.

*leftbottom*
: esquina inferior izquierda.

*rightbottom*
: esquina inferior derecha.

*leftmiddle*
: unión central en el borde izquierdo.

*rightmiddle*
: unión central en el borde derecho.

*centermiddle*
: cruz central.

*horiz*
: línea horizontal.

*vert*
: línea vertical.

*thinhoriz*
: línea horizontal fina.

*thinvert*
: línea vertical fina.

## Compatibilidad <a id="skins-oldcolors"></a>

Compatibilidad de la asignación de colores empleando archivos de skin
con la configuración general de
[Colores](#colors).

La compatibilidad es completa. En este caso la redefinición de colores
tiene prioridad sobre las definiciones de skin y se completa con esta.

# Resaltado de nombres <a id="filenames-highlight"></a>

La sección [filehighlight] de un archivo de skin contiene como claves
los nombres que identificarán cada grupo de resaltado y como valor el
par de colores que le corresponda. El formato de estas parejas se explica
en la sección
[Skins](#skins).

Las reglas de resaltado de nombres en el archivo se encuentran en
*{{pkgdatadir}}/filehighlight.ini.*
Los nombres de sección en este archivo tienen que ser iguales a los nombres
empleados en la sección [filehighlight] del archivo de skin en uso.
PP.
Los nombres de los parámetros en estos grupos podrán ser:

*type*
: tipo de archivo. Si existe se ignoran otras opciones.

*regexp*
: expresión regular. Si existe se ignora la opción 'extensions'.

*extensions*
: lista de extensiones de archivos. Separadas por punto y coma.

*extensions_case*
: hace la regla 'extensions' sensible o no a mayúsculas (true o false).

\`type' puede tomar los valores:

```
- FILE (todos los archivos)
  - FILE_EXE
- DIR (todos los directorios)
  - LINK_DIR
- LINK (todos los enlaces excepto los rotos)
  - HARDLINK
  - SYMLINK
- STALE_LINK
- DEVICE (todos los archivos de dispositivo)
  - DEVICE_BLOCK
  - DEVICE_CHAR
- SPECIAL (todos los archivos especiales)
  - SPECIAL_SOCKET
  - SPECIAL_FIFO
  - SPECIAL_DOOR
```

# Ajustes Especiales <a id="special-settings"></a>

La mayoría de las opciones de M-Commander pueden cambiarse desde
los menús. Sin embargo, hay un pequeño número de ajustes para los que
es necesario editar el archivo de configuración.

Estas variables se pueden cambiar en nuestro archivo
*~/.config/mc6/ini:*

*clear_before_exec*
: Por defecto M-Commander limpia la pantalla antes de ejecutar un
comando. Si preferimos ver la salida del comando en la parte inferior
de la pantalla, editaremos nuestro archivo
*~/mc.ini*
y cambiaremos el
valor del campo clear_before_exec a 0.

*confirm_view_dir*
: Al pulsar F3 en un directorio, normalmente M-Commander entra
en ese directorio.  Si este valor está a 1, entonces el programa
nos pedirá confirmación antes de cambiar el directorio si tenemos
archivos marcados.

*ftpfs_retry_seconds*
: Este valor es el número de segundos que M-Commander esperará
antes de intentar volver a conectar con un servidor de ftp que ha denegado el
acceso. Si el valor es cero, el programa no reintentará el acceso.

*max_dirt_limit*
: Especifica cuántas actualizaciones de pantalla pueden saltarse al menos en el visor
de archivos interno. Normalmente este valor no es significativo, porque
el código automáticamente ajusta el número de actualizaciones a saltar de acuerdo
al volumen de pulsaciones de teclas recibidas. Empero, en máquinas muy lentas o
en terminales con autorepetición de teclado rápida, un valor grande puede hacer
que la pantalla se actualice dando saltos.

> Parece ser que poniendo max_dirt_limit a 10 produce el mejor comportamiento,
> y este es el valor por defecto.

*mouse_move_pages_viewer*
: Controla cuándo el desplazamiento de pantalla realizado con el ratón se realiza por páginas o línea
a línea en el visor de archivos interno.

*only_leading_plus_minus*
: Produce un tratamiento especial para '+', '-', '\*' en la línea de órdenes (seleccionar,
deseleccionar, selección inversa) solo si la línea de órdenes está vacía. No necesitamos
entrecomillar estos caracteres en la línea de órdenes. Pero no podremos
cambiar la selección cuando la línea de órdenes no esté vacía.

*show_output_starts_shell*
: Esta variable solo funciona si no se utiliza el soporte de subshell.
Cuando utilizamos la combinación
*Ctrl-o*
para volver a la pantalla de usuario, si está activada, tendremos un
nuevo shell.  De otro modo, pulsando cualquier tecla nos devolverá a
M-Commander.

*timeformat_recent*
: Cambiar el formato de fecha y hora empleado para fechas dentro de los seis
últimos meses.  Véanse las páginas de manual de strftime o date para la descripción
del formato a emplear.  Sin esta opción se emplea el formato por defecto.

*timeformat_old*
: Cambiar el formato de fecha y hora empleado para fechas más antiguas que seis
meses.  Véanse las páginas de manual de strftime o date para la descripción del formato a
emplear. Sin esta opción se emplea el formato por defecto.

*use_file_to_guess_type*
: Si esta variable está activada (por defecto lo está) se recurrirá al
comando «file» para reconocer los tipos de archivo referidos en el archivo
[extensions.ini](#edit-extension-file).

*xtree_mode*
: Si esta variable está activada (por defecto no) cuando naveguemos
por el sistema de archivos en un panel en árbol, se irá actualizando
automáticamente el otro panel con los contenidos del directorio
seleccionado en cada momento.

*fish_directory_timeout*
: Tiempo de vida por defecto de la caché de directorio. El valor por defecto
de 900 segundos.

*clipboard_store*
: Ruta de acceso y opciones a una utilidad de portapapeles externa como 'xclip'
para cargar texto de un archivo como selección en X Window.
Por ejemplo:

<!-- -->

```
clipboard_store=/usr/bin/xclip -i
```

*clipboard_paste*
: Ruta de acceso y opciones a una utilidad de portapapeles externa como 'xclip'
para volcar la selección de X Window a la salida estándar.
Por ejemplo:

<!-- -->

```
clipboard_paste=/usr/bin/xclip -o
```

*autodetect_codeset*
: Esta opción permite emplear la orden 'enca' para autodetectar el juego de
caracteres de los archivos de texto para el visor y el editor interno. La
lista de valores posibles se puede obtener con
\`enca --list languages | cut -d : -f1'.  Esta opción tiene que estar
dentro de la sección [Misc].

For example:

```
autodetect_codeset=russian
```

# Parámetros para editor o visor externo <a id="parameters-for-external-editor-or-viewer"></a>

M-Commander permite especificar opciones para editores y visores
externos.  M-Commander busca la sección
*[External editor or viewer parameters]*
en el archivo de inicialización del sistema
**{{pkgdatadir}}/defaults.ini**
o en el del usuario
**~/.config/mc6/ini**.
El nombre de la opción debe coincidir con el nombre (ruta completa) del editor
o visor externo.  Su valor puede contener las siguientes variables:

*%filename*
: El nombre del archivo a editar/ver.

*%lineno*
: La línea de comienzo donde abrir el archivo.

Por ejemplo:

```
[External editor or viewer parameters]
    vi=%filename +%lineno
    joe=%filename +%lineno
    more=%filename +%lineno
```

La línea de comienzo solo se pasa al editor o visor externo cuando se llama
desde la ventana de resultados de
[buscar archivo](#find-file).

Si el editor o visor externo se lanza mediante las teclas F3/F4, M-Commander confía en que
el programa (al menos «joe», pero puede que otros también) se comporte abriendo por
defecto el archivo donde se abrió la última vez.  M-Commander no evita que el editor o visor
externo pueda guardar y restaurar posiciones en los archivos abiertos.

# Ajustes del Terminal <a id="terminal-databases"></a>

M-Commander permite hacer ajustes a la base de datos de terminales
del sistema sin necesidad de privilegios de superusuario. El programa
busca definiciones de teclas en el archivo de inicialización del sistema
**{{pkgdatadir}}/defaults.ini**
o en el del usuario
**~/.config/mc6/ini**,
en la sección "terminal:nuestro-terminal" y si no en "terminal:general".
Cada línea comienza con el identificador de la tecla, seguido de un signo
de igual y la definición de la tecla. Para representar el carácter de escape
se utiliza \\e y ^x para el carácter control-x.

Los identificadores de tecla son:

```
f0 a f20      teclas de función f0 a f20
bs            tecla de borrado
home          tecla de inicio
end           tecla de fin
up            tecla de cursor arriba
down          tecla de cursor abajo
left          tecla de cursor izquierda
right         tecla de cursor derecha
pgdn          tecla de avance de página
pgup          tecla de retroceso de página
insert        tecla de insertar
delete        tecla de suprimir
complete      tecla para completar
```

Ejemplo: para indicar que la secuencia Escape + [ + O + p corresponde
a la tecla de insertar, hay que colocar en el archivo
**~/.config/mc6/ini**:

```
insert=\e[Op
```

También se pueden usar
*secuencias avanzadas.*
Por ejemplo:

```
    ctrl-alt-right=\e[[1;6C
    ctrl-alt-left=\e[[1;6D
```

Esto significa que Ctrl + Alt + Izquierda envía la secuencia de escape
\\e[[1;6D y que entonces M-Commander debe interpretar "\\e[[1;6D"
como Ctrl-Alt-Izquierda.

El identificador
*complete*
representa la secuencia usada para invocar el mecanismo de completar
nombres. Esto se hace habitualmente con
*Alt-Tab,*
pero podemos configurar otras teclas para esta función, especialmente en
teclados que incorporan tantas teclas especiales (bonitas pero inútiles
o infrautilizadas).

<!-- help:break -->

# ARCHIVOS AUXILIARES <a id="files"></a>

Los directorios indicados a continuación pueden variar de una
instalación a otra.  También se pueden modificar con la variable de
entorno
**MC_DATADIR**,
que de estar definida se emplearía en vez de {{pkgdatadir}}.

*{{pkgdatadir}}/help/mcommander.md*
: Archivo de ayuda.

*{{pkgdatadir}}/extensions.ini*
: Archivo de extensiones por defecto del sistema.

*~/.config/mc6/extensions.ini*
: Archivo de usuario de extensiones y configuración de visor y editor. Si
está presente prevalece sobre el contenido de los archivos del sistema.

*{{pkgdatadir}}/mc.ini*
: Archivo de configuración del sistema para M-Commander, solo si
el usuario no dispone de su propio
*~/.config/mc6/ini.*

*{{pkgdatadir}}/defaults.ini*
: Opciones globales de M-Commander. Se aplican siempre a todos los
usuarios, tengan
*~/.config/mc6/ini*
o no. Actualmente solo se emplea para los
[ajustes de terminal](#terminal-databases).

*~/.config/mc6/ini*
: Configuración personal del usuario. Si este archivo está presente entonces
se cargará la configuración desde aquí en lugar de desde el archivo de
configuración del sistema.

*{{pkgdatadir}}/hints/hint*
: Este archivo contiene los mensajes cortos de ayuda mostrados por el
programa.

*{{pkgdatadir}}/usermenu*
: Este archivo contiene el menú de aplicaciones por defecto para el sistema.

*~/.config/mc6/menu*
: Menú de aplicaciones personal del usuario. Si está presente será utilizado
en lugar del menú por defecto del sistema.

*~/.cache/mc6/Tree*
: La lista de directorios para el árbol de directorios y la vista en árbol.

*./.usermenu*
: Menú local definido por el usuario. Si este archivo
está presente será usado en lugar del menú de aplicaciones
personal o de sistema.

Para cambiar el directorio de incio de M-Commander se puede utilizar la variable de
entorno
**MC_PROFILE_ROOT**.
El valor de MC_PROFILE_ROOT tiene que ser una ruta absoluta. Si MC_PROFILE_ROOT
no existe o está vacía se usa la variable HOME. Si HOME no existe o está vacía
se recurre a la biblioteca GLib para obtener los directorios de M-Commander.

# LICENCIA <!-- help:skip -->

Este programa se distribuye en los términos que recoge la Licencia Pública
General de GNU (GNU General Public License) tal como fue publicada por
la Fundación de Software Libre (Free Software Foundation). La ayuda
integrada con el programa contiene detalles sobre la Licencia y la
carencia de garantía.

# DISPONIBILIDAD <a id="availability"></a>

La última versión de este programa se puede encontrar en
<https://github.com/blue-panels/mcommander/releases> .

# VÉASE TAMBIÉN <a id="see-also"></a>

mcedit6(1), sh(1), bash(1), tcsh(1), zsh(1), ed(1), view(1),
terminfo(1), gpm(1).

```
La página web de M-Commander está en:
	https://github.com/blue-panels/mcommander
```

La presente documentación recoge información relativa a la versión 4.8
(mayo de 2015).  Esta traducción no está completamente actualizada con
la versión original en inglés.  Para acceder a información sobre
versiones recientes consultar la página de manual en inglés que contiene
información más completa y actualizada.  Para ver el susodicho manual
original ejecutar en la línea de órdenes:

```
        LANG= LC_ALL= man mcommander
```

# AUTORES <a id="authors"></a>

Los autores y contribuciones se recogen en el archivo AUTHORS de la
distribución.

# ERRORES <a id="bugs"></a>

Véase el archivo "TODO" en la distribución para saber qué falta por hacer.

Para informar de problemas con el programa, introducir una nueva incidencia en
<https://github.com/blue-panels/mcommander/issues> .

Se debe proporcionar una descripción detallada del problema, la
versión del programa (que se obtiene con
*'mcommander -V')*
y el sistema operativo utilizados.  Si el programa «revienta», sería
también útil disponer del estado de la pila.

# TRADUCCIÓN <a id="translation"></a>

Francisco Gabriel Aroca, 1998.  Reformateado y actualizado por David
Martín, 2002-2015.

M-Commander traducido a castellano por David Martín.
