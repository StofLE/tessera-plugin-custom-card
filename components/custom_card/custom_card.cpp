#include "custom_card.h"
#include "icons.h"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace esphome::custom_card {

using tessera::Font;
using tessera::Tone;
namespace ui = tessera::ui;

// ---- What a template sensor describes ----
//
// The sensor's attribute `elements` is a list of texts, one part each, its fields split by "|":
//
//   value|<value>|<unit>|<label>|<colour>|<icon>
//   bar|<value>|<unit>|<label>|<colour>|<icon>|<min>|<max>
//   text|<words>|||<colour>|<icon>
//   row|<value>|<unit>|<label>|<colour>|<icon>
//   ring|<value>|<unit>|<label>|<colour>|<icon>|<min>|<max>
//   image|<mark>|<source>|<label>|<colour>|<icon>|<fit>
//
// An image is a picture where a value would stand (plugin API 0.10): <source> is an image entity without its "image."
// (ztr_gescannt for /api/image_proxy/image.ztr_gescannt) or a picture of Home Assistant's own that starts with "/"
// (/local/poster.jpg). <mark> changes with the picture, so the address changes too (?v=<mark>), and it is what the card
// shows as a value where there are no pictures (a board without the memory for them). <fit> is
// contain (the default: the whole picture, as a number drawn by a template) or cover (a photo that fills its place).
//
// An icon field that starts with "#" is a badge instead, as a line's number in the departures plugin: the words after
// it in a box of the part's colour (the accent where it has none), e.g. row|4|min|Main station|blue|#16.
//
// Fields at the end may be left out. A colour is one of the key colours of theme.h (red, orange, yellow, green, mint,
// blue, purple, pink, gray): it paints the part's icon (a dot where it has none) and its bar, never its words. The
// colours that mean something are Tessera's tones (plugin API 0.9), readable on the card in both looks: green is GOOD,
// yellow and orange BUSY, red ALERT, blue ACCENT; the others keep their key colour. An icon is a name of Tessera's set
// (icons.h). Rows that follow each other make a table: each row's label at the left, its value at the right, so the
// values of a list stand under each other.

// The tone a colour name means, as Tessera colours a level: good, moderate, bad, or the accent.
static Tone tone_of(const std::string &name) {
  if (name == "green") return Tone::GOOD;
  if (name == "yellow" || name == "orange") return Tone::BUSY;
  if (name == "red") return Tone::ALERT;
  if (name == "blue") return Tone::ACCENT;
  return Tone::NORMAL;
}

// WCAG's contrast of two colours, 1 to 21.
static float contrast(uint32_t a, uint32_t b) {
  auto lum = [](uint32_t c) {
    float sum = 0;
    const float weight[3] = {0.2126f, 0.7152f, 0.0722f};
    for (int i = 0; i < 3; ++i) {
      const float v = ((c >> (16 - 8 * i)) & 0xFF) / 255.0f;
      sum += weight[i] * (v <= 0.04045f ? v / 12.92f : std::pow((v + 0.055f) / 1.055f, 2.4f));
    }
    return sum;
  };
  const float x = lum(a), y = lum(b);
  return (std::max(x, y) + 0.05f) / (std::min(x, y) + 0.05f);
}

// A part's colour as its icon: Tessera's own tile icon colour, and darker still where it would not read on the card
// (a yellow on white): Tessera's icons sit on a pale disc of their colour, these stand on the card itself.
static uint32_t readable(uint32_t colour) {
  const uint32_t card = theme::hex(theme::CARD);
  for (int i = 0; i < 8 && contrast(colour, card) < 3.0f; ++i) colour = theme::mix(colour, theme::hex(theme::INK), 205);
  return colour;
}
static uint32_t icon_colour(uint32_t colour) { return readable(theme::icon(colour)); }

static constexpr int MAX_PARTS = 8;
static constexpr int MAX_IN_ROW = 4;

enum class Kind : uint8_t { VALUE, BAR, TEXT, ROW, RING, IMAGE };

// A value, or a picture that stands where a value would: both are laid out as values, side by side in their line.
static bool valued(Kind kind) { return kind == Kind::VALUE || kind == Kind::IMAGE; }

// Fixed fields, so the list of parts is one block in PSRAM (tessera::List, plugin API 0.10): Tessera cuts a part's
// text at 48 bytes, so no field is longer.
struct Part {
  Kind kind = Kind::VALUE;
  char value[49] = "", unit[25] = "", label[49] = "";
  uint32_t colour = 0;  // a key colour of theme.h, 0 for the card's own paint
  Tone tone = Tone::NORMAL;  // the tone the colour means, NORMAL for a colour without one
  uint32_t icon = 0;    // a codepoint of Tessera's set, 0 for none
  char badge[9] = "";   // a badge's words ("16", "S3"), "" for none
  float min = 0, max = 100;
  char source[49] = ""; // an image's picture: an image entity without "image.", or a path of Home Assistant's own
  bool cover = false;   // an image fills its place (cover) instead of showing whole (contain)
};

// The address of an image's picture: Home Assistant's own, with its mark so a new picture is a new address (Tessera
// asks again only for another address). The mark is percent-encoded where it is more than letters, digits and . - _ ~.
static std::string picture_url(const Part &part) {
  if (!part.source[0]) return "";
  std::string url = part.source[0] == '/' ? std::string(part.source) : "/api/image_proxy/image." + std::string(part.source);
  if (!part.value[0]) return url;
  url += url.find('?') == std::string::npos ? "?v=" : "&v=";
  static const char *HEX = "0123456789ABCDEF";
  for (const char *c = part.value; *c; ++c) {
    const unsigned char b = static_cast<unsigned char>(*c);
    if (isalnum(b) || b == '.' || b == '-' || b == '_' || b == '~') {
      url += static_cast<char>(b);
    } else {
      url += '%';
      url += HEX[b >> 4];
      url += HEX[b & 15];
    }
  }
  return url;
}

template <size_t N> static void copy_to(char (&to)[N], const std::string &from) {
  const size_t n = std::min(from.size(), N - 1);
  memcpy(to, from.data(), n);
  to[n] = '\0';
}

static std::string trim(const std::string &text) {
  const size_t start = text.find_first_not_of(" \t\r\n");
  if (start == std::string::npos) return "";
  return text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}

static std::vector<std::string> fields_of(const std::string &line) {
  std::vector<std::string> out;
  size_t start = 0;
  while (true) {
    const size_t bar = line.find('|', start);
    out.push_back(trim(line.substr(start, bar == std::string::npos ? std::string::npos : bar - start)));
    if (bar == std::string::npos) return out;
    start = bar + 1;
  }
}

// An icon of Tessera's set by its name, with or without "mdi:"; 0 for one the screen's font does not hold.
static uint32_t icon_of(std::string name) {
  if (name.rfind("mdi:", 0) == 0) name = name.substr(4);
  if (name.empty()) return 0;
  const IconName *first = std::begin(ICON_NAMES), *last = std::end(ICON_NAMES);
  const IconName *at = std::lower_bound(first, last, name.c_str(),
                                        [](const IconName &icon, const char *key) { return strcmp(icon.name, key) < 0; });
  return at != last && name == at->name ? at->codepoint : 0;
}

// "21.4", "21,4" or " 820 " as a number; false for words (unknown, unavailable).
static bool number_of(std::string text, float *out) {
  std::replace(text.begin(), text.end(), ',', '.');
  text = trim(text);
  if (text.empty()) return false;
  char *end = nullptr;
  const float value = strtof(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(value)) return false;
  *out = value;
  return true;
}

static bool parse(const std::string &line, Part *part) {
  const std::vector<std::string> f = fields_of(line);
  auto at = [&](size_t i) { return i < f.size() ? f[i] : std::string(); };
  if (f[0] == "value") part->kind = Kind::VALUE;
  else if (f[0] == "bar") part->kind = Kind::BAR;
  else if (f[0] == "text") part->kind = Kind::TEXT;
  else if (f[0] == "row") part->kind = Kind::ROW;
  else if (f[0] == "ring") part->kind = Kind::RING;
  else if (f[0] == "image") part->kind = Kind::IMAGE;
  else return false;
  copy_to(part->value, at(1));
  if (part->kind == Kind::IMAGE) {
    // An image's third field is its picture, and the seventh how it fits; it has no unit and no range.
    copy_to(part->source, at(2));
    part->cover = at(6) == "cover";
  } else {
    copy_to(part->unit, at(2));
  }
  copy_to(part->label, at(3));
  part->colour = theme::key_swatch(at(4));
  part->tone = tone_of(at(4));
  if (at(5).rfind('#', 0) == 0) copy_to(part->badge, at(5).substr(1));
  else part->icon = icon_of(at(5));
  float n;
  if (number_of(at(6), &n)) part->min = n;
  if (number_of(at(7), &n)) part->max = n;
  return true;
}

// ---- The tile ----
//
// As Tessera's own plugins draw: words and numbers in the look's ink, a part's colour only as a mark (its icon, a dot
// where it has none, and its bar), the value's face chosen for the shape of the value so it never jumps a size when a
// digit changes, and the arrangement chosen for the shape of the tile: a tile one row high and two or more columns
// wide puts every part side by side, any other stacks them.

// The parts of a card on one drawing area: the tile's, or the card's a tap opens over the page. Both get the same data,
// the sensor's state, name and attributes.
class Drawing {
 public:
  explicit Drawing(const tessera::Plugin *plugin) : plugin_(plugin) {}

  // `titled`: the tile's name heads the parts (a card has its title in Tessera's frame instead, and a tile may turn it
  // off). `coloured`: a value takes its part's colour too, not only its mark. `per_row`: how many values stand in a
  // row, 0 for what makes them largest, MAX_IN_ROW for all in one row.
  // `pictures`: images show their pictures (a tile on a board with the memory for them), else their marks as values.
  void create(lv_obj_t *parent, int width, int height, bool flat, const std::string &name, bool titled,
              bool coloured, int per_row = 0, bool pictures = false) {
    per_row_ = per_row;
    pictures_ = pictures;
    width_ = width;
    height_ = height;
    flat_ = flat;
    name_ = roomy::text(name);
    titled_ = titled;
    coloured_ = coloured;
    head_ = ui::label(parent, Font::TITLE, theme::INK);
    note_ = ui::label(parent, Font::BODY, theme::MUTED);
    lv_obj_set_style_text_align(note_, LV_TEXT_ALIGN_CENTER, 0);
    for (Slot &s : slots_) {
      s.icon = ui::label(parent, Font::ICON_SMALL, theme::MUTED);
      s.dot = ui::block(parent, theme::MUTED);
      lv_obj_set_style_radius(s.dot, LV_RADIUS_CIRCLE, 0);
      // A badge's words inside its box, as the departures plugin keeps a line's number in its badge.
      s.badge = ui::label(s.dot, Font::TITLE, theme::ON_ACCENT);
      // A ring as Tessera's meter draws one: 240 degrees on a track, open at the bottom.
      s.arc = lv_arc_create(parent);
      lv_obj_remove_style(s.arc, nullptr, LV_PART_KNOB);
      lv_obj_remove_flag(s.arc, LV_OBJ_FLAG_CLICKABLE);
      lv_arc_set_rotation(s.arc, 150);
      lv_arc_set_bg_angles(s.arc, 0, 240);
      lv_arc_set_range(s.arc, 0, 1000);
      lv_obj_set_style_arc_rounded(s.arc, false, LV_PART_MAIN);
      lv_obj_set_style_arc_rounded(s.arc, false, LV_PART_INDICATOR);
      lv_label_set_long_mode(s.badge, LV_LABEL_LONG_CLIP);
      lv_obj_set_style_text_align(s.badge, LV_TEXT_ALIGN_CENTER, 0);
      s.label = ui::label(parent, Font::BODY, theme::MUTED);
      s.value = ui::label(parent, Font::VALUE, theme::INK);
      s.unit = ui::label(parent, Font::BODY, theme::MUTED);
      s.track = ui::block(parent, theme::TRACK);
      s.fill = ui::block(s.track, theme::ACCENT);
      // A place for an image's picture (plugin API 0.10); hidden, it asks for nothing.
      if (pictures_) s.picture = ui::picture(parent);
    }
    hide_all();
  }

  // What the sensor says now: its parts, laid out and painted.
  void take(JsonObjectConst data) {
    parts_.clear();
    note_text_.clear();
    if (!titled_) shown_name_.clear();
    else if (!name_.empty()) shown_name_ = name_;
    else shown_name_ = data["name"] | "";
    const std::string wait = data["wait"] | "";
    JsonVariantConst elements = data["attributes"]["elements"];
    bool described = false;
    if (elements.is<JsonArrayConst>()) {
      described = true;
      for (JsonVariantConst line : elements.as<JsonArrayConst>()) {
        Part part;
        if (parts_.size() < MAX_PARTS && line.is<const char *>() && parse(line.as<const char *>(), &part))
          parts_.push_back(part);
      }
    } else if (elements.is<const char *>()) {
      described = true;
      Part part;
      if (parse(elements.as<const char *>(), &part)) parts_.push_back(part);
    }
    const std::string state = data["state"] | "";
    if (!wait.empty()) {
      parts_.clear();
      note_text_ = plugin_->text(wait == "wrong_entity" ? "wrong_entity" : "waiting");
    } else if (described && parts_.empty()) {
      note_text_ = plugin_->text("no_elements");
    } else if (!described && state.empty()) {
      note_text_ = plugin_->text("choose");
    } else if (!described) {
      // A sensor without `elements`: its own state and unit, as a plain value.
      Part part;
      copy_to(part.value, state);
      copy_to(part.unit, std::string(data["attributes"]["unit_of_measurement"] | ""));
      parts_.push_back(part);
    }
    layout();
    paint();
  }

  // The colours: words and numbers follow the look; a part's own colour paints its mark, its bar and its ring, and with
  // the tile's option (coloured_) its value too, made readable on the card as its icon is.
  void paint() {
    ui::set_color(head_, theme::INK);
    ui::set_color(note_, theme::MUTED);
    for (size_t i = 0; i < MAX_PARTS; ++i) {
      Slot &s = slots_[i];
      const uint32_t own = i < parts_.size() ? parts_[i].colour : 0;
      const Tone tone = i < parts_.size() ? parts_[i].tone : Tone::NORMAL;
      ui::set_color(s.label, theme::MUTED);
      ui::set_color(s.unit, theme::MUTED);
      ui::set_color(s.value, theme::INK);
      lv_obj_set_style_bg_color(s.track, ui::color(theme::TRACK), 0);
      if (tone != Tone::NORMAL) {
        // The tone's colour; on the icon darker still where it would not read on the card (BUSY's amber on white).
        const uint32_t toned = lv_color_to_u32(ui::tone_color(tone, theme::MUTED));  // 0xFFRRGGBB: the alpha byte is ignored
        lv_obj_set_style_text_color(s.icon, theme::rgb(readable(toned)), 0);
        if (coloured_ && parts_[i].kind != Kind::TEXT) lv_obj_set_style_text_color(s.value, theme::rgb(readable(toned)), 0);
        ui::set_fill(s.dot, tone, theme::ACCENT);
        ui::set_fill(s.fill, tone, theme::ACCENT);
      } else if (own) {
        lv_obj_set_style_text_color(s.icon, theme::rgb(icon_colour(own)), 0);
        if (coloured_ && parts_[i].kind != Kind::TEXT) lv_obj_set_style_text_color(s.value, theme::rgb(icon_colour(own)), 0);
        lv_obj_set_style_bg_color(s.dot, theme::rgb(own), 0);
        lv_obj_set_style_bg_color(s.fill, theme::rgb(own), 0);
      } else {
        ui::set_color(s.icon, theme::MUTED);
        lv_obj_set_style_bg_color(s.fill, ui::color(theme::ACCENT), 0);
      }
      // A ring: its arc on the track in the part's colour (the accent without one).
      const uint32_t mark = tone != Tone::NORMAL ? lv_color_to_u32(ui::tone_color(tone, theme::ACCENT))  // alpha ignored
                            : own ? own : theme::hex(theme::ACCENT);
      lv_obj_set_style_arc_color(s.arc, ui::color(theme::TRACK), LV_PART_MAIN);
      lv_obj_set_style_arc_color(s.arc, theme::rgb(mark), LV_PART_INDICATOR);
      if (i < parts_.size() && parts_[i].badge[0]) {
        // A badge: its box in the part's colour or the accent, its words in whichever of the look's own colours
        // reads best on it.
        const uint32_t box = tone != Tone::NORMAL ? lv_color_to_u32(ui::tone_color(tone, theme::ACCENT))  // the alpha byte is ignored
                             : own ? own : theme::hex(theme::ACCENT);
        lv_obj_set_style_bg_color(s.dot, theme::rgb(box), 0);
        uint32_t words = theme::hex(theme::ON_ACCENT);
        for (theme::Role role : {theme::INK, theme::CARD})
          if (contrast(theme::hex(role), box) > contrast(words, box)) words = theme::hex(role);
        lv_obj_set_style_text_color(s.badge, theme::rgb(words), 0);
      }
    }
  }

 private:
  struct Slot {
    lv_obj_t *icon{}, *dot{}, *label{}, *value{}, *unit{}, *track{}, *fill{}, *badge{}, *arc{}, *picture{};
  };
  // Parts side by side in columns, or a bar or a text alone across the width (`across`: its words left, its value
  // right, the bar under them).
  struct Line {
    int first, count;
    bool across;
  };
  using Lines = tessera::List<Line>;

  static void show(lv_obj_t *obj, bool shown) {
    if (!obj) return;
    if (shown) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
  }

  void hide_all() {
    show(head_, false);
    show(note_, false);
    for (Slot &s : slots_)
      for (lv_obj_t *obj : {s.icon, s.dot, s.label, s.value, s.unit, s.track, s.badge, s.arc, s.picture}) show(obj, false);
  }

  // Stacked: values that follow each other side by side (at most `in_row`), a bar and a text alone across the width.
  Lines stacked(int in_row = MAX_IN_ROW) const {
    Lines out;
    for (int i = 0; i < (int) parts_.size(); ++i) {
      // Values and rings side by side (two counts and their share as a ring), a bar and a text alone across.
      const bool value = valued(parts_[i].kind) || parts_[i].kind == Kind::RING;
      if (value && !out.empty() && !out.back().across && out.back().count < in_row)
        ++out.back().count;
      else
        out.push_back({i, 1, !value});
    }
    return out;
  }
  // Side by side: every part a column of its own, on a wide tile one row high.
  Lines side_by_side() const {
    Lines out;
    if (flat_ && !parts_.empty() && parts_.size() <= MAX_IN_ROW) out.push_back({0, (int) parts_.size(), false});
    return out;
  }
  // The columns of an arrangement: as many as its fullest row of values, so a shorter row's values stand under the
  // columns of the one above it instead of spreading over the width.
  static int columns(const Lines &all) {
    int most = 1;
    for (const Line &line : all)
      if (!line.across) most = std::max(most, line.count);
    return most;
  }

  // The value's digits as zeros: the face is chosen for the shape of a value, never for its digits (as the departures
  // plugin does), so a value that ticks from 999 to 965 keeps its size.
  static std::string shape(std::string text) {
    for (char &c : text)
      if (c >= '1' && c <= '9') c = '0';
    return text;
  }

  // Larger faces first: DIGITS, then Font's own order.
  static int rank(Font f) { return f == Font::DIGITS ? -1 : static_cast<int>(f); }
  // Whether DIGITS has every character of every value shown large (a word, a letter or a sign it lacks keeps VALUE).
  // Only the values of an arrangement's lines of values count: a row, a bar or a text across the width stands in the
  // words' face, so a time on a row ("15:25") never keeps the counts above it small.
  bool digits_hold_values(const Lines &all) const {
    for (const Line &line : all) {
      if (line.across) continue;
      for (int i = line.first; i < line.first + line.count; ++i)
        if (parts_[i].kind != Kind::TEXT && !ui::covers(parts_[i].value, Font::DIGITS)) return false;
    }
    return true;
  }
  int words_row() const { return std::max(ui::line_height(Font::BODY), ui::line_height(Font::ICON_SMALL)); }
  // The words under the rings of a line: as tall as an icon only where one of them has one, else the label alone.
  int ring_row(const Line &line) const {
    for (int i = line.first; i < line.first + line.count; ++i)
      if (parts_[i].icon) return words_row();
    return ui::line_height(Font::BODY);
  }
  // The words of a text, a row and a bar across the width: one step above the body (or larger, words_, where the card
  // has the room), and the body itself where the values had to go down to it, so a tall list of texts shrinks with
  // them instead of running out of the tile.
  Font words_face(Font face) const { return face == Font::BODY ? Font::BODY : words_; }
  // How many lines a text across the width takes: two where it is longer than the width and the layout lets words
  // stay whole (whole_texts_), else one that ends in dots.
  int text_lines(const Part &part, Font face) const {
    if (part.kind != Kind::TEXT || !whole_texts_) return 1;
    const int mark = has_mark(part) ? mark_width(part, Font::ICON_SMALL) + ui::px(6) : 0;
    return mark + ui::text_width(part.value, words_face(face)) > width_ ? 2 : 1;
  }
  // A text's or a row's height: its words, and the small icon's height only where it has a mark.
  int text_row(const Part &part, Font face) const {
    const int words = ui::line_height(words_face(face)) * text_lines(part, face);
    return has_mark(part) ? std::max(words, ui::line_height(Font::ICON_SMALL)) : words;
  }
  // A ring where the layout lets parts be rings (rings_), else its number as a value with its mark.
  bool as_ring(const Part &part) const { return part.kind == Kind::RING && rings_; }
  // One ring wherever it stands, on a card of rings or among values: its value in its middle, its icon and label in its
  // opening, as large as the tile has room for.
  bool meter(const Part &part) const { return as_ring(part); }
  // A ring of a card of rings holds its value and unit in its middle, in the values' face, with its icon and label
  // under it (Schumey's design, pull request #2): the ring is the circle around the value's shape (so it keeps its size
  // when a digit ticks) and never smaller than a key, its stroke the meter's (a fourteenth). The rings of one line are as
  // large as its largest, so their labels stand on one row.
  static int ring_thick(int dia) { return std::max(ui::px(4), dia / 14); }
  int natural_dia(const Part &part, Font face) const {
    const float w = static_cast<float>(ui::text_width(shape(part.value), face) + unit_width(part));
    const float g = 0.62f * static_cast<float>(ui::line_height(face));  // about the digits' height
    const int inner = static_cast<int>(std::ceil(std::sqrt(w * w + g * g))) + 2 * ui::px(4);
    return std::max(ui::px(48), inner + 2 * std::max(ui::px(4), inner / 12));
  }
  // Where the tile has room left, layout() makes the rings larger than their values need (ring_min_).
  int ring_dia(const Part &part, Font face) const { return std::max(ring_min_, natural_dia(part, face)); }
  // The face of the values in the rings: the one layout() found larger for them (ring_face_), else the values' own.
  Font inner_face(Font face) const { return rank(ring_face_) < rank(face) ? ring_face_ : face; }
  // Where a ring's words (`row` high) start, from its top: at the bottom of its open end, under the ends of its arc
  // (three quarters down), but never over its value.
  int ring_words_at(int dia, int row, Font face) const {
    const int lh = ui::line_height(inner_face(face));
    return std::min(dia, std::max({3 * dia / 4, dia - row, (dia - lh) / 2 + lh}));
  }
  // Whether a line has values beside its rings (two counts and their share).
  bool beside_values(const Line &line) const {
    for (int i = line.first; i < line.first + line.count; ++i)
      if (parts_[i].kind != Kind::RING) return true;
    return false;
  }
  // The rings of a line are as large as its largest. Beside values a ring asks only for a ring around its number in the
  // bold face of names, so it never makes the values smaller: it grows into the room the tile has afterwards (ring_min_),
  // at most half again as tall as the values' line, so on a large tile it stays in step with the words beside it (they
  // have no face larger than DIGITS) and the room left goes between the lines; its number takes the largest face that
  // ring holds (ring_face).
  int ring_dia(const Line &line, Font face) const {
    const bool beside = beside_values(line);
    const int most = 3 * (words_row() + ui::line_height(face)) / 2;
    int dia = 0;
    for (int i = line.first; i < line.first + line.count; ++i) {
      if (!meter(parts_[i])) continue;
      const int least = natural_dia(parts_[i], Font::TITLE);
      dia = std::max(dia, beside ? std::max(least, std::min(ring_min_, most)) : ring_dia(parts_[i], face));
    }
    return dia;
  }
  // The face of a ring's number beside values: the largest its ring holds, never larger than the values' own.
  Font ring_face(const Part &part, int dia, Font face) const {
    for (Font f : {Font::DIGITS, Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE}) {
      if (rank(f) < rank(face) || (f == Font::DIGITS && !ui::covers(part.value, Font::DIGITS))) continue;
      if (natural_dia(part, f) <= dia) return f;
    }
    return Font::BODY;
  }
  int bar_thickness() const { return ui::px(ui::large() ? 10 : 6); }
  int dot_size() const { return ui::px(ui::large() ? 12 : 8); }
  int gap() const { return ui::px(ui::large() ? 8 : 4); }
  int column_gap() const { return ui::px(ui::large() ? 16 : 8); }
  int column_width(int count) const { return (width_ - (count - 1) * column_gap()) / count; }

  // A part's mark: its icon, or a dot in its colour where it has no icon, or nothing.
  bool has_mark(const Part &part) const { return part.icon || part.colour || part.badge[0]; }
  // A badge's box: its words in the bold face of names with room on both sides, as the departures plugin's.
  static int badge_pad() { return ui::px(ui::large() ? 8 : 5); }
  static int badge_width(const Part &part) { return ui::text_width(part.badge, Font::TITLE) + 2 * badge_pad(); }
  bool has_words(const Line &line) const {
    for (int i = line.first; i < line.first + line.count; ++i)
      if (parts_[i].label[0] || has_mark(parts_[i])) return true;
    return false;
  }
  // A mark in front of a value, when there is no row for the words above it: the tile's icon size where the value is
  // that tall, else the small one.
  static Font inline_icon(Font face) {
    return ui::line_height(Font::ICON) <= ui::line_height(face) ? Font::ICON : Font::ICON_SMALL;
  }
  int mark_width(const Part &part, Font icon_face) const {
    if (part.badge[0]) return badge_width(part);
    return part.icon ? ui::text_width(ui::icon(part.icon), icon_face) : part.colour ? dot_size() : 0;
  }
  int unit_width(const Part &part) const {
    return !part.unit[0] ? 0 : ui::px(3) + ui::text_width(part.unit, Font::BODY);
  }

  // An image shows its picture where the drawing has pictures and the image an address, else its mark as a value.
  bool pictured(const Part &part) const { return pictures_ && part.kind == Kind::IMAGE && part.source[0]; }
  bool has_picture(const Line &line) const {
    if (line.across) return false;
    for (int i = line.first; i < line.first + line.count; ++i)
      if (pictured(parts_[i])) return true;
    return false;
  }
  // A picture of a mark is a number a template drew tight around its ink, in a face like the screen's digits: its
  // shape is the mark's in DIGITS (VALUE for a mark DIGITS lacks), the ink about 0.62 of the face's line high.
  static Font mark_face(const Part &part) { return ui::covers(part.value, Font::DIGITS) ? Font::DIGITS : Font::VALUE; }
  static float mark_ink(const Part &part) { return 0.62f * static_cast<float>(ui::line_height(mark_face(part))); }
  // How wide a picture `h` high is, at most its column: a number as wide as its shape, so it stands at the left under
  // its label; a picture without a mark, or one that fills its place (cover), takes the column.
  static int picture_width(const Part &part, int h, int column) {
    if (part.cover || !part.value[0]) return column;
    const float w = static_cast<float>(ui::text_width(part.value, mark_face(part))) * h / mark_ink(part);
    return std::max(1, std::min(column, static_cast<int>(std::lround(w))));
  }
  // The width a picture may take of its column: beside another part a twentieth of the drawing's width less (at least a
  // column's gap), so two numbers drawn to the edges of their pictures stand apart in step with the tile, wide on a
  // wide tile and never closer than the columns' words.
  int picture_room(const Line &line, int column) const {
    return line.count > 1 ? column - std::max(column_gap(), width_ / 20) : column;
  }
  // How tall a picture may grow before it fills its column; any height for a picture that takes the column.
  static int picture_height(const Part &part, int column) {
    if (part.cover || !part.value[0]) return 1 << 20;
    const int w = std::max(1, ui::text_width(part.value, mark_face(part)));
    return static_cast<int>(column * mark_ink(part) / w);
  }

  int line_height(const Line &line, Font face, bool labels) const {
    const Part &first = parts_[line.first];
    if (line.across)
      return first.kind == Kind::BAR ? ui::line_height(words_face(face)) + ui::px(4) + bar_thickness()
                                     : text_row(first, face);
    const int words = labels && has_words(line) ? words_row() : 0;
    int h = 0;
    for (int i = line.first; i < line.first + line.count; ++i) {
      switch (parts_[i].kind) {
        case Kind::VALUE:
        case Kind::IMAGE:
        case Kind::ROW: h = std::max(h, words + ui::line_height(face)); break;
        case Kind::RING:
          // A ring keeps its icon and label under it whatever the labels of the values do: it has nowhere else to put a
          // mark, and without them nothing says what its number is.
          if (meter(parts_[i])) {
            const int dia = ring_dia(line, face);
            h = std::max(h, has_words(line) ? std::max(dia, ring_words_at(dia, ring_row(line), face) + ring_row(line)) : dia);
          } else {
            h = std::max(h, words + ui::line_height(face));
          }
          break;
        case Kind::BAR: h = std::max(h, words + ui::line_height(face) + ui::px(4) + bar_thickness()); break;
        default: h = std::max(h, text_row(parts_[i], face));
      }
    }
    return h;
  }

  // Whether every value of `all` has the room of its column in `face`, whatever the height.
  bool fits_across(const Lines &all, Font face, bool labels) const {
    const int column = column_width(columns(all));
    for (const Line &line : all) {
      if (line.across) {
        // A row's value whole at the right, its label taking what is left.
        const Part &part = parts_[line.first];
        // Where the words grow (strict_rows_), the label whole beside it too.
        if (part.kind == Kind::ROW &&
            (has_mark(part) ? mark_width(part, Font::ICON_SMALL) + ui::px(6) : 0) +
                    ui::text_width(part.value, words_face(face)) + unit_width(part) +
                    (strict_rows_ ? ui::px(8) + ui::text_width(part.label, words_face(face)) : 0) > width_)
          return false;
        continue;
      }
      for (int i = line.first; i < line.first + line.count; ++i) {
        const Part &part = parts_[i];
        if (meter(part)) {
          // Its value is in the ring and its words under it: the ring alone.
          if (ring_dia(line, face) > column) return false;
          continue;
        }
        if (part.kind == Kind::TEXT) {
          // A text beside other parts (a flat tile) has its words whole where another arrangement allows it.
          if ((whole_texts_ || strict_rows_) && line.count > 1 && mark_width(part, Font::ICON_SMALL) + (has_mark(part) ? ui::px(6) : 0) +
                                        ui::text_width(part.value, words_face(face)) > column)
            return false;
          continue;
        }
        const int mark = labels || !has_mark(part) ? 0 : mark_width(part, inline_icon(face)) + ui::px(6);
        if (mark + ui::text_width(shape(part.value), face) + unit_width(part) > column) return false;
      }
    }
    return true;
  }

  bool fits(const Lines &all, Font face, bool head, bool labels) const {
    if (!fits_across(all, face, labels)) return false;
    int used = head ? ui::line_height(Font::TITLE) + gap() : 0;
    for (const Line &line : all) used += line_height(line, face, labels);
    used += gap() * ((int) all.size() - 1);
    return used <= height_;
  }

  struct Try { bool head, labels; };
  // The arrangements a search tries: side by side, stacked, and stacked two to a row (four values on a wide flat tile
  // as two rows of two, where that gives them a larger face than one row of four).
  static constexpr int ARRANGEMENTS = 3;
  // With the tile's option (per_row_) only the arrangements of that many values to a row.
  // Two to a row is never the layout's own choice for rings among values: it would part a share from its counts (two
  // counts and their share stand together; the tile's option may still ask for it).
  void arrange() {
    bool ring = false, value = false;
    for (const Part &part : parts_) {
      ring = ring || (part.kind == Kind::RING && rings_);
      value = value || valued(part.kind) || part.kind == Kind::BAR;
    }
    arranged_[0] = per_row_ == 0 || per_row_ >= MAX_IN_ROW ? side_by_side() : Lines();
    arranged_[1] = stacked(per_row_ ? per_row_ : MAX_IN_ROW);
    arranged_[2] = per_row_ == 0 && !(ring && value) ? stacked(2) : Lines();
  }
  // What a search found: whether anything fits, the values' face, whether the name and the labels stand, whether texts
  // beside other parts stay whole, and which arrangement (an index of arranged_).
  struct Pick {
    bool found = false;
    Font face = Font::BODY;
    Try chosen = {false, false};
    bool whole = false;
    int index = 1;
  };
  // The largest face for the values with the name alone (larger is earlier in Font, DIGITS before them all, and only
  // where every value is digits it holds), with rings as rings or as values. Labels before whole words: first with a
  // text beside other parts whole, then with it cut short. Where no label is needed (every part has a mark), the marks
  // stand before the numbers, and the row of labels only where it gives the values a larger face: a mark moves up to
  // its label then, so four values in one row of a wide tile have their column's whole width.
  Pick search(bool rings, bool named, bool unmarked) {
    rings_ = rings;
    arrange();
    Pick best;
    for (bool labelled : {unmarked, !unmarked}) {
      Pick pick;
      for (bool whole : {true, false}) {
        whole_texts_ = whole;
        for (int index = 0; index < ARRANGEMENTS; ++index) {
          const Lines &all = arranged_[index];
          if (all.empty()) continue;
          for (bool head : {named, false}) {
            const Font every[] = {Font::DIGITS, Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY};
            for (Font f : every) {
              if (f == Font::DIGITS && !digits_hold_values(all)) continue;
              if (!fits(all, f, head, labelled)) continue;
              if (!pick.found || rank(f) < rank(pick.face))
                pick = {true, f, {head, labelled || fits(all, f, head, true)}, whole, index};
              break;
            }
            if (pick.found) break;
          }
        }
        if (pick.found) break;
      }
      // Never at the cost of the name or of words kept whole.
      if (pick.found && (!best.found || (rank(pick.face) < rank(best.face) && (pick.whole || !best.whole) &&
                                         (pick.chosen.head || !best.chosen.head))))
        best = pick;
      if (best.found && unmarked) return best;
    }
    return best;
  }

  // Lay the parts out as the departures plugin does: the value takes the largest face that fits beside the tile's name,
  // and the row of labels stays only where it costs the value nothing (else a part's mark stands before its number).
  // The name goes only where no face fits with it. A wide flat tile puts the parts side by side when the value is no
  // smaller there than stacked.
  void layout() {
    hide_all();
    if (!note_text_.empty()) {
      const int h = ui::line_height(Font::BODY);
      lv_obj_set_size(note_, width_, h);
      lv_obj_set_pos(note_, 0, (height_ - h) / 2);
      ui::set_text(note_, roomy::plain(note_text_));
      show(note_, true);
      return;
    }
    words_ = Font::BODY_LARGE;
    ring_min_ = 0;
    ring_face_ = Font::BODY;
    const bool named = !shown_name_.empty();
    // A part whose label has no icon or colour to stand in for it keeps the row of labels, whatever it costs: without
    // it nothing would say what its number is.
    bool unmarked = false, rings = false, values = false;
    for (const Part &part : parts_) {
      // A ring's colour says how good it is, not what it is: a ring keeps its label unless it has an icon (Schumey).
      if (part.kind != Kind::TEXT && part.label[0] && !(part.kind == Kind::RING ? part.icon != 0 : has_mark(part)))
        unmarked = true;
      if (part.kind == Kind::RING) rings = true;
      if (valued(part.kind) || part.kind == Kind::BAR) values = true;
    }
    // Rings drawn as rings first. Where no arrangement takes them (a flat tile), or where they would make the card's
    // other values smaller than they are without them, their numbers stand as values.
    Pick pick = search(true, named, unmarked);
    bool rings_chosen = true;
    // Rings that only fit without the tile's name give way to their numbers as values where those keep it: the name
    // goes only where nothing fits with it (a 2x1 of rings shows its name and the values, not small rings alone).
    const bool nameless = named && pick.found && !pick.chosen.head;
    if (rings && (!pick.found || values || nameless)) {
      const Pick plain = search(false, named, unmarked);
      if (plain.found && (!pick.found || rank(plain.face) < rank(pick.face) || (nameless && plain.chosen.head))) {
        pick = plain;
        rings_chosen = false;
      }
    }
    // Where nothing fits at all, the fallback below shows numbers, never a ring too large for the tile.
    if (!pick.found) rings_chosen = false;
    rings_ = rings_chosen;
    arrange();
    const bool found = pick.found;
    Font face = pick.face;
    const Try chosen = pick.chosen;
    const Lines *lines = &arranged_[pick.index];
    // What the lines below are measured with: the choice that fitted, else texts on one line.
    whole_texts_ = pick.whole;
    // Rows and texts a step larger (HEADLINE; TITLE is the bold face of names, no larger) where the card has room for
    // them: never larger than its values, and never a row's label or a text beside other parts cut short for it.
    if (found) {
      if (!values || rank(Font::HEADLINE) >= rank(face)) {
        strict_rows_ = true;
        words_ = Font::HEADLINE;
        if (!fits(*lines, face, chosen.head, chosen.labels)) words_ = Font::BODY_LARGE;
        strict_rows_ = false;
      }
    }
    // The rings of a card of rings as large as the tile has room for, all of one size, and their values in the largest
    // face their rings hold: the face that fitted is only the least they get.
    bool meters = false;
    for (const Part &part : parts_) meters = meters || meter(part);
    if (found && meters) {
      int lo = 0, hi = width_;
      while (lo < hi) {
        ring_min_ = (lo + hi + 1) / 2;
        if (fits(*lines, face, chosen.head, chosen.labels)) lo = ring_min_;
        else hi = ring_min_ - 1;
      }
      ring_min_ = lo;
      bool digits = true;
      for (const Part &part : parts_)
        if (meter(part) && !ui::covers(part.value, Font::DIGITS)) digits = false;
      const Font every[] = {Font::DIGITS, Font::VALUE, Font::HEADLINE, Font::TITLE, Font::BODY_LARGE, Font::BODY};
      for (Font f : every) {
        if (rank(f) >= rank(face) || (f == Font::DIGITS && !digits)) continue;
        bool held = true;
        for (const Line &line : *lines)
          for (int i = line.first; i < line.first + line.count; ++i)
            if (meter(parts_[i]) && natural_dia(parts_[i], f) > ring_dia(line, face)) held = false;
        // A larger face pushes the words lower: the tile still has to hold them.
        ring_face_ = f;
        if (held && fits(*lines, face, chosen.head, chosen.labels)) break;
        ring_face_ = Font::BODY;
      }
    }
    // Where nothing fits, the values two to a row, else one, so that no number is cut short; the lines that fit whole
    // show (below).
    if (!found)
      for (int in_row : {2, 1}) {
        fallback_ = stacked(in_row);
        lines = &fallback_;
        if (fits_across(fallback_, face, false)) break;
      }

    int y = 0;
    if (chosen.head) {
      const int h = ui::line_height(Font::TITLE);
      lv_obj_set_size(head_, width_, h);
      lv_obj_set_pos(head_, 0, 0);
      ui::set_text(head_, roomy::plain(shown_name_));
      show(head_, true);
      y = h + gap();
    }
    int content = 0;
    for (const Line &line : *lines) content += line_height(line, face, chosen.labels);
    const int spare = std::max(0, height_ - y - content);
    const int n = (int) lines->size();
    const int cols = columns(*lines);
    // The first line with pictures takes the room the tile has left, until its pictures fill their columns; what they
    // do not take goes between the lines, as on a card without pictures.
    const Line *growing = nullptr;
    int grow = 0;
    if (found)
      for (const Line &line : *lines)
        if (has_picture(line)) {
          growing = &line;
          break;
        }
    if (growing) {
      int most = 1 << 20;
      for (int i = growing->first; i < growing->first + growing->count; ++i)
        if (pictured(parts_[i])) most = std::min(most, picture_height(parts_[i], picture_room(*growing, column_width(cols))));
      const int room = std::max(0, spare - gap() * (n - 1));
      grow = std::max(0, std::min(room, most - ui::line_height(face)));
    }
    const int left = spare - grow;
    const int between = n > 1 ? std::max(gap(), left / (n + 1)) : 0;
    y += std::max(0, (left - between * (n - 1)) / 2);
    // Where even the smallest face does not fit, the lines that fit whole, never one cut in half at the bottom.
    for (const Line &line : *lines) {
      const int extra = &line == growing ? grow : 0;
      const int h = line_height(line, face, chosen.labels) + extra;
      if (!found && y + h > height_) break;
      place(line, cols, face, chosen.labels, y, extra);
      y += h + between;
    }
  }

  // A part's mark at (x, y), centred on a row `h` high, in `icon_face`; returns its width.
  int place_mark(Slot &s, const Part &part, int x, int y, int h, Font icon_face) {
    if (part.badge[0]) {
      // The box as tall as the bold face and a little more, at most the row; its words centred in it.
      const int w = badge_width(part), th = ui::line_height(Font::TITLE);
      const int bh = std::min(h, th + ui::px(ui::large() ? 6 : 4));
      lv_obj_set_size(s.dot, w, bh);
      lv_obj_set_pos(s.dot, x, y + (h - bh) / 2);
      lv_obj_set_style_radius(s.dot, ui::px(ui::large() ? 6 : 4), 0);
      show(s.dot, true);
      ui::set_font(s.badge, Font::TITLE);
      ui::set_text(s.badge, part.badge);
      lv_obj_set_size(s.badge, w, th);
      lv_obj_center(s.badge);
      show(s.badge, true);
      return w;
    }
    if (part.icon) {
      const std::string glyph = ui::icon(part.icon);
      const int w = ui::text_width(glyph, icon_face), ih = ui::line_height(icon_face);
      ui::set_font(s.icon, icon_face);
      ui::set_text(s.icon, glyph);
      lv_obj_set_size(s.icon, w, ih);
      lv_obj_set_pos(s.icon, x, y + (h - ih) / 2);
      show(s.icon, true);
      return w;
    }
    if (part.colour) {
      const int d = dot_size();
      lv_obj_set_style_radius(s.dot, LV_RADIUS_CIRCLE, 0);
      lv_obj_set_size(s.dot, d, d);
      lv_obj_set_pos(s.dot, x, y + (h - d) / 2);
      show(s.dot, true);
      return d;
    }
    return 0;
  }

  // The mark and words beside it, centred on a row `h` high at (x, y), at most `room` wide and `lines` lines high (a
  // label of several lines wraps at its width and ends the last in dots, as ui::text does).
  void place_words(Slot &s, const Part &part, int x, int y, int h, int room, Font words_face, lv_obj_t *words,
                   const std::string &text, int lines = 1) {
    const int mark = place_mark(s, part, x, y, std::min(h, ui::line_height(words_face)), Font::ICON_SMALL);
    const int left = x + (mark ? mark + ui::px(6) : 0);
    if (!text.empty() && x + room - left > 0) {
      const int wh = ui::line_height(words_face) * lines;
      ui::set_font(words, words_face);
      ui::set_text(words, text);
      lv_obj_set_size(words, x + room - left, wh);
      lv_obj_set_pos(words, left, y + (h - wh) / 2);
      show(words, true);
    }
  }

  // A value and its unit after it, on the value's baseline.
  void place_value(Slot &s, const Part &part, Font face, int x, int y, int room) {
    const int uw = unit_width(part);
    const int vw = std::min(ui::text_width(part.value, face), std::max(1, room - uw));
    const int vh = ui::line_height(face);
    ui::set_font(s.value, face);
    ui::set_text(s.value, part.value);
    lv_obj_set_size(s.value, vw, vh);
    lv_obj_set_pos(s.value, x, y);
    show(s.value, true);
    if (part.unit[0]) {
      const lv_font_t *big = ui::font(face), *small = ui::font(Font::BODY);
      const int uh = ui::line_height(Font::BODY);
      const int baseline = y + vh - (big ? big->base_line : 0);
      ui::set_font(s.unit, Font::BODY);
      ui::set_text(s.unit, part.unit);
      lv_obj_set_size(s.unit, uw - ui::px(3), uh);
      lv_obj_set_pos(s.unit, x + vw + ui::px(3), baseline - uh + (small ? small->base_line : 0));
      show(s.unit, true);
    }
  }

  // A ring in a column at (x, y) of a line `h` high: the arc filled from min to max, open at the bottom as Tessera's
  // meter's, the value and unit in its middle, and under it the icon and the label (`words`), all centred on the line.
  // The ring's colour marks it, so it needs no dot.
  void place_ring(Slot &s, const Part &part, const Line &line, int x, int y, int column, int h, Font face, bool words) {
    const int dia = ring_dia(line, face), thick = ring_thick(dia), cx = x + column / 2;
    const int row_h = words ? ring_row(line) : 0;
    const int at = words ? ring_words_at(dia, row_h, face) : dia;
    const int top = y + std::max(0, (h - std::max(dia, at + row_h)) / 2);
    lv_obj_set_size(s.arc, dia, dia);
    lv_obj_set_pos(s.arc, cx - dia / 2, top);
    lv_obj_set_style_arc_width(s.arc, thick, LV_PART_MAIN);
    lv_obj_set_style_arc_width(s.arc, thick, LV_PART_INDICATOR);
    float value, share = 0;
    if (number_of(part.value, &value) && part.max > part.min)
      share = std::min(1.0f, std::max(0.0f, (value - part.min) / (part.max - part.min)));
    lv_arc_set_value(s.arc, static_cast<int>(std::lround(share * 1000)));
    show(s.arc, true);
    const Font inner = beside_values(line) ? ring_face(part, dia, face) : inner_face(face);
    const int vw = std::min(dia - 2 * thick, ui::text_width(part.value, inner) + unit_width(part));
    place_value(s, part, inner, cx - vw / 2, top + (dia - ui::line_height(inner)) / 2, vw);
    if (!words) return;
    const int mark = part.icon ? ui::text_width(ui::icon(part.icon), Font::ICON_SMALL) : 0;
    const int space = mark && part.label[0] ? ui::px(6) : 0;
    const int row = std::min(column, mark + space + (part.label[0] ? ui::text_width(part.label, Font::BODY) : 0));
    const int lx = cx - row / 2, ly = top + at;
    if (mark) place_mark(s, part, lx, ly, row_h, Font::ICON_SMALL);
    if (part.label[0] && row - mark - space > 0) {
      const int lh = ui::line_height(Font::BODY);
      ui::set_font(s.label, Font::BODY);
      ui::set_text(s.label, part.label);
      lv_obj_set_size(s.label, row - mark - space, lh);
      lv_obj_set_pos(s.label, lx + mark + space, ly + (row_h - lh) / 2);
      show(s.label, true);
    }
  }

  // A bar `width` wide at (x, y), filled from min to max.
  void place_bar(Slot &s, const Part &part, int x, int y, int width) {
    const int thick = bar_thickness();
    lv_obj_set_size(s.track, width, thick);
    lv_obj_set_pos(s.track, x, y);
    lv_obj_set_style_radius(s.track, thick / 2, 0);
    lv_obj_set_style_radius(s.fill, thick / 2, 0);
    show(s.track, true);
    float value, share = 0;
    if (number_of(part.value, &value) && part.max > part.min)
      share = std::min(1.0f, std::max(0.0f, (value - part.min) / (part.max - part.min)));
    const int filled = share > 0 ? std::max(thick, (int) std::lround(share * width)) : 0;
    lv_obj_set_size(s.fill, filled, thick);
    lv_obj_set_pos(s.fill, 0, 0);
    show(s.fill, filled > 0);
  }

  // `extra`: the height a line of pictures has above its own (layout()), all of it for its pictures.
  void place(const Line &line, int cols, Font face, bool labels, int y, int extra = 0) {
    const int h = line_height(line, face, labels) + extra;
    if (line.across) {
      const Part &part = parts_[line.first];
      Slot &s = slots_[line.first];
      if (part.kind == Kind::TEXT) {
        place_words(s, part, 0, y, h, width_, words_face(face), s.value, part.value, text_lines(part, face));
        return;
      }
      if (part.kind == Kind::ROW) {
        // A row of a table: its mark and label at the left, its value and unit at the right, all in the words' face,
        // so the values of rows that follow each other stand in one column.
        const Font words = words_face(face);
        const int right = ui::text_width(part.value, words) + unit_width(part);
        const int vh = ui::line_height(words);
        place_value(s, part, words, width_ - right, y + (h - vh) / 2, right);
        place_words(s, part, 0, y, h, std::max(0, width_ - right - ui::px(8)), words, s.label, part.label);
        return;
      }
      // A bar across the width: its words at the left, the value at the right, the bar under them.
      const Font words = words_face(face);
      const int row = ui::line_height(words);
      const int right = ui::text_width(part.value, words) + unit_width(part);
      place_value(s, part, words, width_ - right, y, right);
      place_words(s, part, 0, y, row, std::max(0, width_ - right - ui::px(8)), Font::BODY, s.label, part.label);
      place_bar(s, part, 0, y + row + ui::px(4), width_);
      return;
    }
    const int column = column_width(cols);
    const bool words = labels && has_words(line);
    for (int k = 0; k < line.count; ++k) {
      const Part &part = parts_[line.first + k];
      Slot &s = slots_[line.first + k];
      const int x = k * (column + column_gap());
      if (meter(part)) {
        place_ring(s, part, line, x, y, column, h, face, has_words(line));
        continue;
      }
      if (part.kind == Kind::TEXT) {
        const int th = text_row(part, face);
        place_words(s, part, x, y + (h - th) / 2, th, column, words_face(face), s.value, part.value);
        continue;
      }
      // Beside a ring taller than they are, the values stand in the middle of the line.
      const int picture = pictured(part) ? ui::line_height(face) + extra : 0;
      const int own = (words ? words_row() : 0) + (picture ? picture : ui::line_height(face)) +
                      (part.kind == Kind::BAR ? ui::px(4) + bar_thickness() : 0);
      const int top = y + std::max(0, (h - own) / 2);
      int vy = top;
      int room = 0;
      if (words) {
        place_words(s, part, x, top, words_row(), column, Font::BODY, s.label, part.label);
        vy += words_row();
      } else if (has_mark(part)) {
        // No row for the words: the mark stands before the number, centred on it, and says what it is.
        room = place_mark(s, part, x, top, ui::line_height(face), inline_icon(face)) + ui::px(6);
      }
      if (picture && s.picture) {
        // The picture where the number would stand, from its left: whole and on the card's colour (contain, a
        // number), or filling its place with a key's corners (cover, a photo).
        lv_obj_set_size(s.picture, picture_width(part, picture, picture_room(line, column) - room), picture);
        lv_obj_set_pos(s.picture, x + room, vy);
        lv_obj_set_style_radius(s.picture, part.cover ? ui::px(ui::large() ? 10 : 6) : 0, 0);
        ui::set_picture(s.picture, picture_url(part), part.cover ? tessera::Fit::COVER : tessera::Fit::CONTAIN);
        show(s.picture, true);
        continue;
      }
      place_value(s, part, face, x + room, vy, column - room);
      if (part.kind == Kind::BAR) place_bar(s, part, x, vy + ui::line_height(face) + ui::px(4), column);
    }
  }

  const tessera::Plugin *plugin_;
  int width_ = 0, height_ = 0;
  bool flat_ = false, titled_ = true, coloured_ = false, pictures_ = false;
  int per_row_ = 0;  // the tile's option: values to a row, 0 for the layout's choice
  // Kept in PSRAM (tessera::Text and tessera::List, plugin API 0.10): the memory inside the chip stays Wi-Fi's.
  tessera::Text name_, shown_name_, note_text_;
  tessera::List<Part> parts_;
  Lines fallback_;  // the arrangement of a tile where nothing fits, which `lines` may point to
  mutable bool whole_texts_ = true;
  Font words_ = Font::BODY_LARGE;     // the face of texts, rows and bars across, chosen by layout()
  bool rings_ = true;                 // rings drawn as rings (layout() falls back to values where none fits)
  Lines arranged_[ARRANGEMENTS];      // the arrangements of arrange(), for the rings_ that layout() chose
  int ring_min_ = 0;                  // the least a ring of a card of rings is across, chosen by layout()
  Font ring_face_ = Font::BODY;       // the face of the values in those rings, chosen by layout()
  bool strict_rows_ = false;          // while layout() tries larger words: labels and texts beside parts stay whole  // texts whole: beside other parts only where they fit, across in two lines
  Slot slots_[MAX_PARTS];
  lv_obj_t *head_{}, *note_{};
};

// The tile: its parts on the card's area; a tap opens them large over the page.
class CardTile : public tessera::Tile {
 public:
  explicit CardTile(const tessera::Plugin *plugin) : plugin_(plugin), drawing_(plugin) {}

  void create(const tessera::TileContext &c) override {
    entity_ = c.entity ? c.entity : "";
    tile_ = c.tile;
    // A tile one row high and two columns or more puts its parts side by side, as the departures plugin turns into a
    // list on two columns.
    // Its options (Schumey, pull request #2): the name over the parts, and the values in their parts' colours.
    // And how many values stand in a row: the layout's choice, all in one row, or two.
    const std::string per_row = c.options["per_row"] | "auto";
    drawing_.create(c.parent, c.width, c.height, c.rows == 1 && c.columns >= 2, c.name ? c.name : "",
                    c.options["show_name"] | true, c.options["coloured_values"] | false,
                    per_row == "all" ? MAX_IN_ROW : per_row == "two" ? 2 : 0, ui::pictures());
  }
  void on_state(JsonObjectConst data) override { drawing_.take(data); }
  void on_theme() override { drawing_.paint(); }
  void on_tap() override { tessera::open_card(plugin_->plugin_id(), "detail", roomy::plain(entity_), tile_); }

 private:
  const tessera::Plugin *plugin_;
  Drawing drawing_;
  tessera::Text entity_;
  int tile_ = -1;
};

// The card a tap on the tile opens: the same parts, as large as a card's room takes, with their labels where they fit.
// Tessera draws its frame, its back key and the tile's name as its title.
class DetailCard : public tessera::Card {
 public:
  explicit DetailCard(const tessera::Plugin *plugin) : drawing_(plugin) {}

  void open(const tessera::CardContext &c) override {
    // Its images as pictures too, on a board that shows pictures (plugin API 0.13, pictures on a card).
    drawing_.create(c.parent, c.width, c.height, false, "", false, false, 0, ui::pictures());
  }
  void on_state(JsonObjectConst data) override { drawing_.take(data); }
  void on_theme() override { drawing_.paint(); }

 private:
  Drawing drawing_;
};

void CustomCard::setup() {
  add_tile("card", [this]() { return new CardTile(this); });
  add_card("detail", [this]() { return new DetailCard(this); });
}

}  // namespace esphome::custom_card
