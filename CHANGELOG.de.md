# Änderungen

## 0.7.0 - 2026-10-23
- Ein neuer Teil, `image`: ein Bild von Home Assistant, wo sonst ein Wert stünde, etwa eine Zählung, die ein Template
  größer gezeichnet hat als die größten Ziffern des Screens, das Plakat einer Veranstaltung oder das Foto einer Person:
  `image|<Marke>|<Quelle>|<Label>|<Farbe>|<Icon>|<Fit>`. Die Quelle ist eine Bild-Entität ohne ihr `image.`
  (`image|5.358|zaehler_gescannt|Gescannt`) oder ein Pfad von Home Assistant (`/local/plakat.jpg`); die Marke ändert
  sich mit dem Bild, und wo es keine Bilder gibt (ein Board ohne Speicher dafür), zeigt die Karte sie als Wert. Die
  Karte nach einem Tipp zeigt die Bilder ebenfalls, größer in ihrem Platz. Bilder stehen nebeneinander wie Werte, links
  unter ihren Labels, und nehmen den Platz, den die Kachel übrig hat, bis sie ihre Spalten füllen. `contain` (Standard)
  zeigt das ganze Bild auf der Farbe der Karte, `cover` füllt seinen Platz.
- Die README zeigt zu jedem Beispiel ein Bild auf einem Screen, so lässt sich jede Zeile eines Templates dem zuordnen,
  was sie zeichnet (Issue #6), und sagt, dass die ganze Zeile eines Teils zu seinen 48 Bytes zählt, eine lange also
  zuerst ihr Icon verliert.
- Braucht Tessera 0.4.89 oder neuer (Plugin-API 0.13, Bilder auf einer Karte).

## 0.6.1 - 2026-10-18
- Ringe so groß wie die Kachel: Auf einer Karte mit Ringen wachsen sie in den Platz, den die Kachel hat (so hoch wie
  eine breite Kachel mit einer Reihe), alle gleich groß, ihre Werte in der größten Schrift, die die Ringe fassen, und
  das Label steht in der offenen Unterseite seines Rings statt darunter.
- Vier Werte auf einer breiten Kachel mit einer Reihe stehen als zwei Reihen zu zwei, wo sie dadurch größer werden als
  in einer Reihe.
- Werte, deren Teile alle ein Icon haben, nehmen die Zeile der Labels, wo sie dadurch größer werden: Das Icon rückt zu
  seinem Label, so hat jeder Wert die ganze Breite seiner Spalte (vier Werte in einer Reihe unter dem Namen der Kachel).
- Eine dritte Option der Kachel im Inspector: **Werte in einer Reihe**, automatisch (wie bisher), alle in einer Reihe
  oder zwei pro Reihe (vier Werte auf einer breiten Kachel als eine Reihe mit vier oder zwei Reihen mit zwei).
- Diese vier von Schumey (Pull Request #3), danke.
- Ein Ring, wo er auch steht: Ein Ring zwischen Werten (zwei Zählungen und ihr Anteil) ist derselbe Ring wie auf einer
  Karte mit Ringen, sein Wert groß in der Mitte, sein Label in der Öffnung, und er wächst in den Platz der Kachel,
  höchstens anderthalbmal so hoch wie die Zeile der Werte, damit er auf großen Kacheln zu ihnen passt; die Werte daneben
  stehen in der Mitte der Zeile. Der kleine Ring, so hoch wie die Zahlen der Werte, entfällt.
- Ein Ring zwischen Werten kommt nie allein in eine eigene Reihe: Wo eine schmale Kachel neben ihnen keinen Platz für ihn
  hat, steht sein Anteil als Wert unter den Zählungen, die ihre große Schrift behalten.
- Ringe, die nur ohne den Namen der Kachel passen, weichen ihren Zahlen als Werte, wo diese ihn behalten: Eine 2x1-Kachel
  mit Ringen zeigt ihren Namen.

## 0.6.0 - 2026-10-17
- Eine Tabelle: aufeinanderfolgende `row`-Teile stehen als Zeilen, das Label links und der Wert rechts, so stehen die
  Werte einer Liste (Räume, Akkus, Abfahrten) untereinander.
- Eine lange Textzeile nimmt zwei Zeilen, wo die Karte Platz hat, statt mit Punkten zu enden.
- Tabellen- und Textzeilen bekommen eine größere Schrift, wo die Karte Platz hat, nie größer als ihre Werte.
- Ein Ring: `ring`-Teile stehen als Ringe, mehrere nebeneinander (die Luft eines Raums, die Akkus im Haus), der Wert
  groß in der Mitte seines Rings, Icon und Label darunter, alle Ringe einer Reihe so groß wie der größte (Schumeys
  Entwurf, Pull Request #2). Ein Ring behält Icon und Label darunter immer, auch wo Werte ihre für eine größere Schrift
  aufgeben: Seine Farbe sagt, wie gut ein Wert ist, nicht, was er ist. Auf einer Karte mit Werten ist ein Ring kompakt: Er steht zwischen ihnen, so hoch wie ihre Zahlen, mit
  seiner Zahl innen (ein Anteil neben zwei Zählungen). Auf einem zu kleinen Tile stehen ihre Zahlen als Werte.
- Ein Kästchen: Ein Icon-Feld, das mit `#` beginnt, zeigt seinen Text in einem Kästchen in der Farbe des Teils, wie die
  Liniennummer in einer Abfahrtsliste (`row|4|min|Hauptbahnhof|blue|#16`).
- Die Werte einer kürzeren Reihe stehen unter den Spalten der Reihe darüber, statt sich über die Breite zu verteilen.
- Die Karte hält ihre Teile im PSRAM des Bildschirms, so bleibt der Speicher frei, den das WLAN braucht.
- Werte bekommen die großen Ziffern auch neben einer Zeile mit Uhrzeit oder Worten (`row|15:25||Renntag`): Es zählen nur
  die Werte, die groß stehen, so sind zwei Zahlen über einem Anteil und einer Uhrzeit so groß, wie die Karte es zulässt.
- Zwei Optionen der Kachel im Inspector (Schumey, Pull Request #2): **Namen zeigen** (an) und **Werte in ihrer Farbe**
  (aus). Beide lassen eine Kachel standardmäßig so, wie sie war.
- Braucht Tessera 0.4.88 oder neuer.

## 0.5.0 - 2026-10-14
- Große Zahlen auf großen Tiles: Wo sie passen und alle Werte Zahlen sind, bekommen die Werte die großen Ziffern
  der Klimakarte von Tessera. Ein Wert mit einem Wort darin behält seine bisherige Schrift.
- Grün, Gelb, Orange, Rot und Blau sind Tesseras eigene Farben nach Bedeutung (gut, mäßig, schlecht, Akzent), hell und
  dunkel auf der Karte lesbar, wie Tessera einen Wert einfärbt.
- Wo auf einer kleinen Kachel nichts passt, stehen die Werte zu zweit oder einzeln untereinander, statt in einer Zeile
  abgeschnitten zu werden.
- Ein Text neben anderen Teilen auf einer flachen Kachel bleibt ganz, wo die Teile stattdessen untereinander stehen
  können; Labels gehen weiterhin vor.
- Braucht Tessera 0.4.88 oder neuer: eine ältere bietet weiter 0.4.3 an.

## 0.4.3 - 2026-10-12
- Bis zu acht Teile auf einer Karte statt sechs: zwei volle Reihen mit vier Werten. Jede Kachel der Karte kostet den
  Bildschirm dafür etwas mehr Speicher.
- Textzeilen und Balken über die ganze Breite werden auf einer niedrigen Kachel mit den Werten kleiner, statt unten
  herauszulaufen; wo selbst die kleinste Größe zu groß ist, zeigt die Karte die Zeilen, die ganz passen.

## 0.4.2 - 2026-10-12
- Ein Bild auf der Plugins-Seite.

## 0.4.1 - 2026-10-11
- Ein farbiges Icon wird im hellen Modus dunkler, wo es sich sonst nicht von der Karte abhebt: ein gelbes wird auf Weiß ocker.

## 0.4.0 - 2026-10-10
- Ein Tippen auf das Tile öffnet die Teile groß auf einer Karte über der Seite, mit ihren Labels.
- Braucht ein Tessera vom 10.10.2026 oder neuer: ein älteres zeigt die Karte unter ihrer Titelzeile leer.

## 0.3.3 - 2026-10-09
- Auf der Plugins-Seite unter dem Thema Zuhause zu finden.
- Braucht ein Tessera mit Plugin-API 0.7: erst die App aktualisieren.

## 0.3.2 - 2026-10-09
- Ein Label ohne Icon und ohne Farbe bleibt immer, so sagt jede Zahl, was sie ist.
- Ein Icon oder Punkt vor einer Zahl hat etwas mehr Abstand zu ihr.

## 0.3.1 - 2026-10-09
- Der Inspector des Tiles bietet nur noch Sensoren an, die eine Karte beschreiben (mit einem Attribut `elements`).

## 0.3.0 - 2026-10-09
- Erste Version: Werte, Balken und Textzeilen, die ein Template-Sensor beschreibt, nebeneinander auf einem breiten
  Tile und gestapelt auf jedem anderen, in den Schriften und Farben des Bildschirms.
