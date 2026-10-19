# A number as a picture, for the card's `image` part

An example, not part of the plugin: a small service that draws a number (or a short word) as a picture, so a custom
card can show a count larger than the screen's largest digits. Home Assistant fetches the picture through a template
image, and the card's `image` part shows that image entity. Use it as it is, or as a start for your own.

The service draws only what the address says (`/value.png?text=5358&ink=300`): it reads no data, keeps nothing and
needs no access to anything. The picture is the text in Roboto Bold, the face of Tessera's digits, in Tessera's blue,
on a transparent ground, tight around its ink: the card decides how large it stands, and the card's own colour shows
through in the light and the dark look.

## Run it

Any machine Home Assistant can reach works: a small container, a Raspberry Pi, a server in the house.

```sh
sudo apt install python3-pil fonts-roboto
python3 value_image.py --port 8765
```

`--font` takes another TTF. `http://<host>:8765/value.png?text=5358&ink=300` should show the number;
`--preview <folder>` writes a few samples instead of serving.

To keep it running, a systemd unit such as:

```ini
[Unit]
Description=Numbers as pictures for Tessera's custom card
After=network-online.target

[Service]
ExecStart=/usr/bin/python3 /opt/value-image/value_image.py --port 8765
DynamicUser=yes
Restart=on-failure

[Install]
WantedBy=multi-user.target
```

Let only Home Assistant reach the port (a firewall rule for its address).

## In Home Assistant

A template image per number. Its `url` changes with the number, so Home Assistant fetches the new picture; a
trigger every 10 seconds keeps a busy counter from asking for a picture at every single step.

```yaml
template:
  - triggers:
      - trigger: time_pattern
        seconds: "/10"
      - trigger: homeassistant
        event: start
    image:
      - name: Count scanned
        unique_id: count_scanned
        default_entity_id: image.count_scanned
        verify_ssl: false
        url: "http://192.168.1.20:8765/value.png?ink=300&text={{ states('sensor.entrance_scanned') | int(0) }}"
```

In the card's sensor, the part names the image entity without `image.`, with the number as its mark:

```yaml
'image|' ~ (states('sensor.entrance_scanned') | int(0)) ~ '|count_scanned|Scanned'
```

The mark makes the picture's address new when the number changes, and it stands as a value where there are no
pictures. Write the number the same way in the image's `url` and in the mark (here both as `int`), so the picture
and the mark say the same; a thousands separator (`5.358`) goes into both, as `'{:,}'.format(n) | replace(',', '.')`.

## Good to know

- `ink` is the height of the digits in the picture, in pixels. 300 is plenty for a 7-inch screen; the card scales the
  picture down to its place, which keeps the digits sharp.
- `color` takes `accent` (Tessera's blue, the default), `ink` (light grey) or a hex colour (`ffffff`).
- A part's text is at most 48 bytes, so keep the image entity's name short.
