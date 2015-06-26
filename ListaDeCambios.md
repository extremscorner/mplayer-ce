# Lista de cambios de MPlayerCE #

## 0.7 ##

  * Ahora los archivos se analizan al principio, seguido del cache cuando este haya terminado del preanálisis, así que después verás llenarse el caché.
  * Mejorado cache de audio/video por internet, detecta si es un streaming por internet, y si es un stream de radio el caché inicial será pequeño.
  * Optimizado el acceso a la memoria por sugerencia de Shagkur, el código de cahé ahora es más estable.
  * Añadido parche A8 para reproducción de DVD en cIOS202 para evitar problemas con viejos ModChips.
  * Mejorado el libfat y arreglados caracteres especiales.
  * Uso de el último libfat para prevenir la posible corrupción de las tarjetas SD.
  * Mejorada conexión USB  y detección de DVD, modificando el módulo ECHI para una mayor compatibilidad con las memorias USB.
  * Ahora se puede montar cualquier partición FAT, no debe dar ningún problema si la partición está configurada como primaria o activada.
  * Establecido soporte FAT32 así que ahora puedes reproducir archivos de mayor tamaño.
  * Los archivos de vídeo que no habían funcionado antes puede que ahora lo hagan.
  * Implementado nuevo sistema de buffer para cuando el caché comienza a bajar, que para el sistema y realiza un re-buffer para conseguir un valor mínimo de caché, lo que previene posibles cuelgues con malas conexiones. Puedes ver el caché pulsando 1 en el Wiimando, cuando el caché llegue al 3% se parará.
  * Shoutcast TV es completo gracias a Extrems, revisa menu.conf.
  * No es necesario DVDx si se tiene instalado cIOS202 para el acceso a DVD.

## Nuevo Instalador de cIOS ##

  * Ahora MPlayerCE detectará la presencia del IOS202 para decidir si usar el DVDx o no, así que la gente que instale el cIOS202 no necesita instalar DVDx para tener sus funciones. La instalación de DVDx por defecto puede ser usada si instalas IOS202 ahora. El nuevo instalador es para el USB2 únicamente, e instalará IOS202 basándose en IOS60 por sus mejoras del Wi-Fi. Ahora se puede seleccionar el IOS para correr la instalación, así que puedes seleccionar el IOS que posea el bug de firma falsa o bug treucha. (El motivo de que DVDx no sea necesario al tener cIOS202 es que existe un parche en IOS202 que permite acceder al DVD sin DVDx). El cIOS es necesario para MPlayer para usar el driver USB2. Para más detalles lee el LÉEME que forma parte de este paquete.

## 0.62 ##

  * Arreglado (de nuevo) el soporte para el adaptador LAN USB. Gracias a CountZ3ro por el testeo. Ten en cuenta que necesitarás instalar el nuevo cIOS USB 2.0
  * El cIOS mejora los conflictos con otro homewbrew. Ten en cuenta que ahora es el puerto 0 (USB0) el que soporta USB 2.0. Todos los demás dispositivos USB (incluido el adaptador LAN USB) deben ir en el puerto 1 (USB1). Visita este enlace para más detalles: `http://mplayer-ce.googlecode.com/files/usb.jpg`
  * Añadido parámetro de ensanche horizontal (ver mplayer.conf)
  * Añadidas opciones de YouTube al menu.conf (gracias a Extrems)
  * Algunos pequeños bugs reparados

## 0.61 ##

  * Ahora funcionan las nuevas variables del tamaño de la pantalla
  * Mejora en la deteccion de USB
  * Se deja de usar la fuente Arial para usar Liberation (de distribucion libre)
  * Arreglados algunos bugs con los subtitulos
  * Liberada versión en español

## 0.6 ##

  * No hay limite de cache
  * Introducidas nuevas variables en el archivo mplayer.conf para ajustar el tamaño y la posicion de la imagen, por favor consulta el archivo mplayer.conf para mas detalles
  * Añadido soporte para los subtitulos idx/sub (tener en cuenta que puede tardar más de 30 segundos en cargar así que sea paciente
  * Añadidas distintas posibilidades de ubicación de los archivos, ahora se dan las siguientes opciones:
    * sd:/apps/mplayer\_ce
    * sd:/mplayer
    * usb:/apps/mplayer\_ce
    * usb:/mplayer
  * Añadidos puntos de guardado - el video continuará en el ultimo punto que se paró. Para borrarlos, borra el archivo resume\_points de la carpeta mplayer\_ce. Para ir al principio del video pulsa el botón menos mientras sujetas el botón dos.
  * Añadido soporte para el cIOS de Hermes.  Esto proporciona una gran compatibilidad USB y permite conectarte vía USB LAN Connector. Mira el wiki para más detalles.
  * Añadido soporte para librería Fribidi
  * Se ha hecho que el cache sea visible en la pantalla
  * Algunos bugs arreglados
  * Actualizado al último SVN del MPlayer