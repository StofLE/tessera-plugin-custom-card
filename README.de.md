# Eigene Karte

Ein Tile, das du selbst zusammenstellst: Werte, Ringe, Balken, Textzeilen und Tabellen, die ein Template-Sensor in Home
Assistant beschreibt, gezeichnet vom Bildschirm in seinen eigenen Schriften und Farben. Das Raumklima eines Zimmers auf
einem Tile, eine Liste der Räume, die Akkus, die schwach werden, die nächsten Abfahrten an deiner Haltestelle: alles,
was ein Template sagen kann.

Die Karte richtet sich nach dem Tile, auf dem sie steht. Auf einem Tile, das eine Zeile hoch und zwei oder mehr Spalten
breit ist, stehen die Teile nebeneinander; jedes andere Tile stapelt sie: Werte nebeneinander, ein Balken, eine
Textzeile oder eine Tabellenzeile über die ganze Breite. Werte bekommen die größte Schrift, die passt, auf einem großen
Tile die großen Ziffern der Klimakarte; Tabellen- und Textzeilen wachsen ebenfalls mit dem Platz. Ein Tippen öffnet die
Teile groß auf einer Karte über der Seite, mit ihren Labels.

## Einrichten

1. Lege in Home Assistant einen Template-Sensor an, dessen Attribut `elements` die Teile der Karte aufzählt, einen Text
   je Teil (die Beispiele unten), füge ihn in die `configuration.yaml` ein und lade unter Entwicklerwerkzeuge, YAML die
   **Template-Entitäten** neu.
2. Öffne in Tessera **Plugins**, wähle **Eigene Karte** und hake deine Bildschirme an. Jeder Bildschirm baut einmal neu.
3. Platziere im Layout das Tile **Eigene Karte** aus der Gruppe Plugins der Bibliothek, in jeder Größe, und wähle im
   Inspector den Template-Sensor. Er bietet die Sensoren an, die ein Attribut `elements` haben.

## Beispiele

Kopiere eins, ersetze die Entity-IDs durch deine eigenen und lade die Template-Entitäten neu. Jedes gehört unter
`template:` in der `configuration.yaml`.
[examples/living_room.yaml](https://github.com/StofLE/tessera-plugin-custom-card/blob/main/examples/living_room.yaml)
ist ein größeres Beispiel, mit Stufen für Feuchte und CO2.

### Das Raumklima eines Zimmers

Werte mit Icons, die Farbe der Feuchte nach ihrer Stufe und das CO2 als Balken von 400 bis 2000 ppm.

```yaml
template:
  - sensor:
      - name: Wohnzimmer-Karte
        state: "{{ states('sensor.wohnzimmer_temperatur') }}"
        attributes:
          elements: >
            {% set t = states('sensor.wohnzimmer_temperatur') %}
            {% set h = states('sensor.wohnzimmer_feuchte') | int(0) %}
            {% set c = states('sensor.wohnzimmer_co2') | int(0) %}
            {{ [
              'value|' ~ t ~ '|°C|Temperatur|blue|thermometer',
              'value|' ~ h ~ '|%|Feuchte|' ~ ('green' if 40 <= h <= 60 else 'orange') ~ '|water-percent',
              'bar|' ~ c ~ '|ppm|CO2|' ~ ('green' if c < 800 else 'yellow' if c < 1200 else 'red')
                ~ '|molecule-co2|400|2000'
            ] }}
```

### Die Luft eines Raums als Ringe

Ringe nebeneinander, jeder gefüllt vom kleinsten bis zum größten Wert, sein Wert groß in seiner Mitte.

```yaml
template:
  - sensor:
      - name: Luft-Karte
        state: "{{ states('sensor.buero_co2') }}"
        attributes:
          elements: >
            {% set c = states('sensor.buero_co2') | int(0) %}
            {% set h = states('sensor.buero_feuchte') | int(0) %}
            {{ [
              'ring|' ~ c ~ '|ppm|CO2|' ~ ('green' if c < 800 else 'yellow' if c < 1200 else 'red')
                ~ '|molecule-co2|400|2000',
              'ring|' ~ h ~ '|%|Feuchte|' ~ ('green' if 40 <= h <= 60 else 'orange') ~ '|water-percent|0|100',
              'ring|' ~ states('sensor.buero_temperatur') ~ '|°C|Temperatur|blue|thermometer|10|30'
            ] }}
```

### Eine Tabelle der Räume

Aufeinanderfolgende Zeilen stehen als Tabelle: die Namen links, die Werte rechts in einer Spalte.

```yaml
template:
  - sensor:
      - name: Räume-Karte
        state: "{{ states('sensor.aussen_temperatur') }}"
        attributes:
          elements: >
            {% set rooms = [
              ('Küche', 'sensor.kueche_temperatur'),
              ('Schlafzimmer', 'sensor.schlafzimmer_temperatur'),
              ('Bad', 'sensor.bad_temperatur'),
              ('Büro', 'sensor.buero_temperatur'),
              ('Außen', 'sensor.aussen_temperatur')] %}
            {% set ns = namespace(rows=[]) %}
            {% for name, entity in rooms %}
              {% set v = states(entity) %}
              {% set v = '-' if v in ['unknown', 'unavailable'] else v %}
              {% set ns.rows = ns.rows + ['row|' ~ v ~ '|°C|' ~ name] %}
            {% endfor %}
            {{ ns.rows[:8] }}
```

### Die Akkus, die schwach werden

Jeder Akku-Sensor im Haus, der schwächste zuerst, rot unter 20 %, gelb unter 50 %. Home Assistant rendert ein Template,
das alle Sensoren durchgeht, höchstens einmal pro Minute; für Akkus reicht das völlig.

```yaml
template:
  - sensor:
      - name: Akkus-Karte
        state: "{{ now().minute }}"
        attributes:
          elements: >
            {% set ns = namespace(items=[]) %}
            {% for s in states.sensor
                 | selectattr('attributes.device_class', 'defined')
                 | selectattr('attributes.device_class', 'eq', 'battery')
                 | rejectattr('state', 'in', ['unknown', 'unavailable']) %}
              {% set ns.items = ns.items + [(s.state | int(0), s.name)] %}
            {% endfor %}
            {% set out = namespace(rows=[]) %}
            {% for level, name in (ns.items | sort(attribute='0'))[:8] %}
              {% set colour = 'red' if level < 20 else 'yellow' if level < 50 else 'green' %}
              {% set out.rows = out.rows + ['row|' ~ level ~ '|%|' ~ name[:18] ~ '|' ~ colour ~ '|battery-high'] %}
            {% endfor %}
            {{ out.rows }}
```

### Abfahrten mit Liniennummer

Ein Kästchen (ein Icon-Feld, das mit `#` beginnt) zeigt die Nummer einer Linie, wie auf einer Abfahrtstafel. Woher die
Liste kommt, hängt von deiner Integration ab; hier ist es ein Attribut `departures` mit `line`, `destination` und
`minutes` je Abfahrt. Ersetze diese Namen durch die deines Sensors.

```yaml
template:
  - sensor:
      - name: Abfahrten-Karte
        state: "{{ now().minute }}"
        attributes:
          elements: >
            {% set ns = namespace(rows=[]) %}
            {% for d in (state_attr('sensor.hauptbahnhof_abfahrten', 'departures') or [])[:8] %}
              {% set when = 'jetzt|' if d.minutes | int(0) == 0 else d.minutes ~ '|min' %}
              {% set ns.rows = ns.rows + ['row|' ~ when ~ '|' ~ d.destination[:18] ~ '|blue|#' ~ d.line] %}
            {% endfor %}
            {{ ns.rows }}
```

### Zählungen mit Anteil und Textzeile

Zwei Zählungen, ihr Anteil als Balken und wann gezählt wurde. Mit `ring` statt `bar` steht der Anteil als Ring neben den
Zählungen, seine Zahl groß darin, so groß, wie die Kachel Platz hat.

```yaml
template:
  - sensor:
      - name: Einlass-Karte
        state: "{{ states('sensor.einlass_gescannt') }}"
        attributes:
          elements: >
            {% set s = states('sensor.einlass_gescannt') | int(0) %}
            {% set v = states('sensor.einlass_gueltig') | int(0) %}
            {% set p = (100 * s / v) | round(0) | int if v else 0 %}
            {{ [
              'value|' ~ s ~ '||Gescannt',
              'value|' ~ v ~ '||Gültig',
              'bar|' ~ p ~ '|%|Anteil|blue||0|100',
              'text|Stand ' ~ states('sensor.einlass_uhrzeit') ~ '||||clock-outline'
            ] }}
```

## Die Teile

Jeder Teil ist ein Text, seine Felder durch `|` getrennt in dieser Reihenfolge: Art, Wert, Einheit, Label, Farbe, Icon
und bei einem Balken oder Ring sein kleinster und größter Wert. Felder am Ende dürfen fehlen; ein Feld in der Mitte bleibt leer
(`value|3||Offen`).

| Art | Was sie zeigt | Beispiel |
|---|---|---|
| `value` | Eine Zahl oder ein Wort mit Einheit, Label und Icon darüber. | `value\|21,4\|°C\|Temperatur\|blue\|thermometer` |
| `bar` | Eine Zahl mit einem Balken darunter, vom kleinsten bis zum größten Wert (ohne Angabe 0 und 100). | `bar\|820\|ppm\|CO2\|yellow\|molecule-co2\|400\|2000` |
| `text` | Eine Textzeile; eine lange nimmt zwei Zeilen, wo die Karte Platz hat. | `text\|Tür offen\|\|\|\|door-open` |
| `row` | Eine Zeile einer Tabelle: das Label links, der Wert rechts. Aufeinanderfolgende Zeilen stehen als Tabelle. | `row\|20,8\|°C\|Küche` |
| `ring` | Ein Ring, gefüllt vom kleinsten bis zum größten Wert (ohne Angabe 0 und 100), der Wert in seiner Mitte, Icon und Label darunter. Aufeinanderfolgende Ringe stehen nebeneinander, alle so groß wie der größte; neben Werten ist es derselbe Ring, er steht zwischen ihnen und wächst in den Platz der Kachel; wo sie nicht passen, steht die Zahl als Wert. | `ring\|46\|%\|Feuchte\|green\|water-percent\|0\|100` |

- **Farbe**: `red`, `orange`, `yellow`, `green`, `mint`, `blue`, `purple`, `pink` oder `gray`. Wie auf Tesseras eigenen
  Tiles markiert sie einen Teil: Sie färbt das Icon (einen Punkt, wo der Teil keins hat), den Balken, den Ring und ein
  Kästchen, die Schrift bleibt in den Farben des Looks. Die Option **Werte in ihrer Farbe** der Kachel färbt auch die
  Zahlen. `green` gut, `yellow` und `orange` mäßig, `red` schlecht und `blue` die Akzentfarbe sind Tesseras eigene
  Farben, hell und dunkel gut lesbar.
- **Icon**: ein Name aus der Icon-Auswahl des Editors, mit oder ohne `mdi:`.
- **Kästchen**: Ein Icon-Feld, das mit `#` beginnt, zeigt den Text danach in einem Kästchen in der Farbe des Teils (ohne
  Farbe in der Akzentfarbe), etwa die Nummer einer Linie: `row|4|min|Hauptbahnhof|blue|#16`. Höchstens 8 Zeichen.

## Gut zu wissen

- Home Assistant rendert das Template; das Tile ändert sich, sobald sich der Sensor ändert. Stufen und Farben berechnest
  du im Template, dort rundest du die Werte und wählst das Dezimalzeichen.
- Höchstens acht Teile (zwei Reihen mit vier Werten), jeder höchstens 48 Bytes: Tessera kürzt längere Texte, also Labels
  kurz halten (die Beispiele kürzen Namen mit `[:18]`).
- Ist ein Tile zu klein für alle Teile, stehen die Werte zu zweit oder einzeln in einer Reihe, und es erscheinen die
  Zeilen, die ganz passen: keine Zahl wird abgeschnitten, keine Zeile halbiert. Ein Text neben anderen Teilen auf einem
  flachen Tile bleibt ganz, wo die Teile stattdessen untereinander stehen können.
- Die Schrift eines Werts richtet sich nach seiner Form, nicht nach den Ziffern, so behält das Tile seine Größe, wenn
  sich ein Wert ändert.
- Ist der Sensor eine Weile nicht verfügbar, behält das Tile ihn und zeigt seinen Zustand, bis `elements` wieder da ist.
- Ein Plugin-Tile kann keine ganze Seite belegen; jede andere Größe des Rasters geht.
- Die Hintergrundfarbe des Tiles stellst du im Editor ein, wie bei jedem Tile. Sein Inspector hat drei eigene
  Optionen: **Namen zeigen** (an) für den Namen über den Teilen (aus gibt den Teilen seinen Platz), **Werte in ihrer
  Farbe** (aus), um auch jede Zahl in der Farbe ihres Teils zu zeigen, hell und dunkel lesbar, und **Werte in einer
  Reihe** (automatisch): alle Werte und Ringe in einer Reihe oder zu zweit, statt der Anordnung, in der sie am größten
  werden (vier Werte auf einer breiten Kachel als eine Reihe mit vier oder zwei Reihen mit zwei). Die Karte, die ein
  Tippen öffnet, zeigt ihre Zahlen weiter in der normalen Schriftfarbe und wählt ihre Anordnung selbst.

## Wie es funktioniert

- `tessera-plugin.yaml`: ein Tile, das zu einem Sensor gehört und dessen `elements` und `unit_of_measurement` bekommt.
- `components/custom_card/`: die ESPHome-Komponente, die die Teile anordnet und zeichnet, auf dem Tile und auf der
  Karte, die ein Tippen öffnet.
- `translations/`: die Texte auf dem Bildschirm (`screen`) und in der App (`app`).
- `CHANGELOG.de.md`: was jede Version geändert hat; die App zeigt es bei einem Update.
- `tools/make_icons.py`: schreibt die Namen von Tesseras Icon-Satz in `icons.h`.
