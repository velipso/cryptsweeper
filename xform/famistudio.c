//
// cryptsweeper - fight through the crypt to slay the vampire
// by Sean Connelly (@velipso), https://sean.fun
// Project Home: https://github.com/velipso/cryptsweeper
// SPDX-License-Identifier: 0BSD
//

#include "famistudio.h"
#include "stb_ds.h"
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>

void famistudio_help() {
  printf(
    "  famistudio <input.txt> <output.txt>\n"
    "    Convert FamiStudio text export into gvsong source code\n"
  );
}

static int parse_note(const char *str, int octave) {
  int note = octave * 12;
  if (str[0] == 'c' || str[0] == 'C') {
    note += 8;
  } else if (str[0] == 'd' || str[0] == 'D') {
    note += 10;
  } else if (str[0] == 'e' || str[0] == 'E') {
    note += 12;
  } else if (str[0] == 'f' || str[0] == 'F') {
    note += 13;
  } else if (str[0] == 'g' || str[0] == 'G') {
    note += 15;
  } else if (str[0] == 'a' || str[0] == 'A') {
    note += 17;
  } else if (str[0] == 'b' || str[0] == 'B') {
    note += 19;
  } else {
    // unknown note
    return -1;
  }
  int o = 1;
  if (str[1] == '#') {
    note++;
    o = 2;
  }
  if (str[o] >= '0' && str[o] <= '9') {
    note += (str[o] - '0') * 12;
    if (note < 0x08 || note > 0x7f) {
      return -2; // note out of range
    }
    return note;
  }
  // unknown octave
  return -1;
}

static void print_note(int note, char *out) {
  if (note < 0x08 || note > 0x7f) {
    out[0] = '-';
    out[1] = '-';
    out[2] = '-';
    return;
  }
  note -= 8;
  int octave = note / 12;
  out[2] = '0' + octave;
  switch (note % 12) {
    case  0: out[0] = 'C'; out[1] = '-'; return;
    case  1: out[0] = 'C'; out[1] = '#'; return;
    case  2: out[0] = 'D'; out[1] = '-'; return;
    case  3: out[0] = 'D'; out[1] = '#'; return;
    case  4: out[0] = 'E'; out[1] = '-'; return;
    case  5: out[0] = 'F'; out[1] = '-'; return;
    case  6: out[0] = 'F'; out[1] = '#'; return;
    case  7: out[0] = 'G'; out[1] = '-'; return;
    case  8: out[0] = 'G'; out[1] = '#'; return;
    case  9: out[0] = 'A'; out[1] = '-'; return;
    case 10: out[0] = 'A'; out[1] = '#'; return;
    case 11: out[0] = 'B'; out[1] = '-'; return;
  }
}

struct FS_Envelope {
  int loop;
  int release;
  int *values;
};

struct FS_Instrument {
  char *name;
  int expansion;
  int vrc6saw;
  struct FS_Envelope dutycycle;
  struct FS_Envelope volume;
  struct FS_Envelope pitch;
};

struct FS_Note {
  int time;
  int value;
  int duration;
  int release;
  char *instrument;
  int volume;
};

struct FS_Pattern {
  char *name;
  struct FS_Note **notes;
};

struct FS_PatternInstance {
  int time;
  char *pattern;
};

struct FS_Channel {
  struct FS_Pattern **patterns;
  struct FS_PatternInstance **instances;
};

struct FS_Song {
  char *name;
  int length;
  int looppoint;
  int patternlength;
  int beatlength;
  int notelength;
  int *groove;
  struct FS_Channel square1;
  struct FS_Channel square2;
  struct FS_Channel triangle;
  struct FS_Channel noise;
  struct FS_Channel vrc6square1;
  struct FS_Channel vrc6square2;
  struct FS_Channel vrc6saw;
  struct FS_Channel *currentchannel;
  int currentoctave;
};

struct FS_Project {
  struct FS_Instrument **instruments;
  struct FS_Song **songs;
};

static void fs_envelope_print(struct FS_Envelope *envelope) {
  printf(
    "Loop(%d) Release(%d) Values: {",
    envelope->loop,
    envelope->release
  );
  for (int i = 0; i < arrlen(envelope->values); i++) {
    printf("%s%d", i > 0 ? ", " : "", envelope->values[i]);
  }
  printf("}\n");
}

static struct FS_Instrument *fs_instrument_new(const char *name) {
  struct FS_Instrument *instrument = calloc(1, sizeof(struct FS_Instrument));
  instrument->name = strdup(name);
  return instrument;
}

static void fs_instrument_print(struct FS_Instrument *instrument) {
  printf(
    "  Instrument '%s' Expansion(%d) Vrc6Saw(%d):\n",
    instrument->name,
    instrument->expansion,
    instrument->vrc6saw
  );
  printf("    Duty Cycle Envelope: ");
  fs_envelope_print(&instrument->dutycycle);
  printf("    Volume Envelope: ");
  fs_envelope_print(&instrument->volume);
  printf("    Pitch Envelope: ");
  fs_envelope_print(&instrument->pitch);
}

static void fs_instrument_free(struct FS_Instrument *instrument) {
  free(instrument->name);
  arrfree(instrument->dutycycle.values);
  arrfree(instrument->volume.values);
  arrfree(instrument->pitch.values);
  free(instrument);
}

static struct FS_Note *fs_note_new(const char *instrument) {
  struct FS_Note *note = calloc(1, sizeof(struct FS_Note));
  note->instrument = strdup(instrument);
  return note;
}

static void fs_note_print(struct FS_Note *note) {
  char out[4] = {0};
  print_note(note->value, out);
  printf(
    "        Note '%s' Time(%d) Value(%d/%s) Duration(%d) Release(%d) Volume(%d)\n",
    note->instrument,
    note->time,
    note->value,
    out,
    note->duration,
    note->release,
    note->volume
  );
}

static void fs_note_free(struct FS_Note *note) {
  free(note->instrument);
  free(note);
}

static struct FS_Pattern *fs_pattern_new(const char *name) {
  struct FS_Pattern *pattern = calloc(1, sizeof(struct FS_Pattern));
  pattern->name = strdup(name);
  return pattern;
}

static int fs_note_compare(const struct FS_Note **a, const struct FS_Note **b) {
  return (*a)->time - (*b)->time;
}

static void fs_pattern_sort(struct FS_Pattern *pattern) {
  qsort(
    pattern->notes,
    arrlen(pattern->notes),
    sizeof(struct FS_Note *),
    (int (*)(const void *, const void *))fs_note_compare
  );
}

static void fs_pattern_fixduration(struct FS_Pattern *pattern, int patternlength, int notelength) {
  for (int n = 0; n < arrlen(pattern->notes); n++) {
    struct FS_Note *note = pattern->notes[n];
    if (n < arrlen(pattern->notes) - 1) {
      int nexttime = pattern->notes[n + 1]->time;
      if (note->duration > 0 && note->time + note->duration >= nexttime) {
        // next note will stop this note, so unset duration
        note->duration = -1;
      }
    }
    if (note->duration > 0 && note->time + note->duration > patternlength * notelength) {
      // clamp to pattern boundary :-(
      note->duration = patternlength * notelength - note->time;
    }
    if (note->duration > 0 && note->release > note->duration) {
      // release is after duration, so unset it
      note->release = -1;
    }
  }
}

static void fs_pattern_print(struct FS_Pattern *pattern) {
  printf("      Pattern '%s':\n", pattern->name);
  for (int i = 0; i < arrlen(pattern->notes); i++) {
    fs_note_print(pattern->notes[i]);
  }
}

static void fs_pattern_free(struct FS_Pattern *pattern) {
  free(pattern->name);
  for (int i = 0; i < arrlen(pattern->notes); i++) {
    fs_note_free(pattern->notes[i]);
  }
  arrfree(pattern->notes);
  free(pattern);
}

static struct FS_PatternInstance *fs_patterninstance_new(const char *pattern) {
  struct FS_PatternInstance *instance = calloc(1, sizeof(struct FS_PatternInstance));
  instance->pattern = strdup(pattern);
  return instance;
}

static void fs_patterninstance_print(struct FS_PatternInstance *instance) {
  printf("      PatternInstance '%s' Time(%d)\n", instance->pattern, instance->time);
}

static void fs_patterninstance_free(struct FS_PatternInstance *instance) {
  free(instance->pattern);
  free(instance);
}

static int fs_instance_compare(
  const struct FS_PatternInstance **a,
  const struct FS_PatternInstance **b
) {
  return (*a)->time - (*b)->time;
}

static void fs_channel_sort(struct FS_Channel *channel) {
  for (int p = 0; p < arrlen(channel->patterns); p++) {
    fs_pattern_sort(channel->patterns[p]);
  }
  qsort(
    channel->instances,
    arrlen(channel->instances),
    sizeof(struct FS_PatternInstance *),
    (int (*)(const void *, const void *))fs_instance_compare
  );
}

static void fs_channel_fixduration(struct FS_Channel *channel, int patternlength, int notelength) {
  for (int p = 0; p < arrlen(channel->patterns); p++) {
    fs_pattern_fixduration(channel->patterns[p], patternlength, notelength);
  }
}

static void fs_channel_print(struct FS_Channel *channel) {
  for (int i = 0; i < arrlen(channel->patterns); i++) {
    fs_pattern_print(channel->patterns[i]);
  }
  for (int i = 0; i < arrlen(channel->instances); i++) {
    fs_patterninstance_print(channel->instances[i]);
  }
}

static void fs_channel_freebody(struct FS_Channel *channel) {
  for (int i = 0; i < arrlen(channel->patterns); i++) {
    fs_pattern_free(channel->patterns[i]);
  }
  arrfree(channel->patterns);
  for (int i = 0; i < arrlen(channel->instances); i++) {
    fs_patterninstance_free(channel->instances[i]);
  }
  arrfree(channel->instances);
  // NOTE: do *not* free the channel!
}

static struct FS_Song *fs_song_new(const char *name) {
  struct FS_Song *song = calloc(1, sizeof(struct FS_Song));
  song->name = strdup(name);
  song->currentchannel = &song->square1;
  song->currentoctave = 1;
  return song;
}

// calculate tempo based on groove
// https://github.com/BleuBleu/FamiStudio/blob/d87138dd6b40e0c0391615b8be0d8ecf8fbf415d/FamiStudio/Source/Utils/TempoUtils.cs#L123
static int fs_song_tempo(struct FS_Song *song) {
  double gsum = 0;
  int glen = arrlen(song->groove);
  for (int i = 0; i < glen; i++) {
    gsum += song->groove[i];
  }
  int i = 1;
  for (; ((glen * i) % song->beatlength) != 0; i++);
  return round(3600.0 * ((glen * i) / song->beatlength) / (gsum * i));
}

static void fs_song_print(struct FS_Song *song) {
  printf(
    "  Song '%s' Length(%d) LoopPoint(%d) PatternLength(%d) BeatLength(%d) NoteLength(%d) "
    "Tempo(%d) Groove: {",
    song->name,
    song->length,
    song->looppoint,
    song->patternlength,
    song->beatlength,
    song->notelength,
    fs_song_tempo(song)
  );
  for (int i = 0; i < arrlen(song->groove); i++) {
    printf("%s%d", i > 0 ? ", " : "", song->groove[i]);
  }
  printf("}\n");
  printf("    Square1:\n");
  fs_channel_print(&song->square1);
  printf("    Square2:\n");
  fs_channel_print(&song->square2);
  printf("    Triangle:\n");
  fs_channel_print(&song->triangle);
  printf("    Noise:\n");
  fs_channel_print(&song->noise);
  printf("    VRC6 Square1:\n");
  fs_channel_print(&song->vrc6square1);
  printf("    VRC6 Square2:\n");
  fs_channel_print(&song->vrc6square2);
  printf("    VRC6 Saw:\n");
  fs_channel_print(&song->vrc6saw);
}

static void fs_song_free(struct FS_Song *song) {
  free(song->name);
  arrfree(song->groove);
  fs_channel_freebody(&song->square1);
  fs_channel_freebody(&song->square2);
  fs_channel_freebody(&song->triangle);
  fs_channel_freebody(&song->noise);
  fs_channel_freebody(&song->vrc6square1);
  fs_channel_freebody(&song->vrc6square2);
  fs_channel_freebody(&song->vrc6saw);
}

static struct FS_Project *fs_project_new() {
  return calloc(1, sizeof(struct FS_Project));
}

static void fs_project_print(struct FS_Project *project) {
  printf("Project:\n");
  for (int i = 0; i < arrlen(project->instruments); i++) {
    fs_instrument_print(project->instruments[i]);
  }
  for (int i = 0; i < arrlen(project->songs); i++) {
    fs_song_print(project->songs[i]);
  }
}

static void fs_project_free(struct FS_Project *project) {
  for (int i = 0; i < arrlen(project->instruments); i++) {
    fs_instrument_free(project->instruments[i]);
  }
  arrfree(project->instruments);
  for (int i = 0; i < arrlen(project->songs); i++) {
    fs_song_free(project->songs[i]);
  }
  arrfree(project->songs);
  free(project);
}

static const char *get_kv(const char *key, const char *def, char **keys, char **vals) {
  for (int i = 0; i < arrlen(keys); i++) {
    if (strcmp(keys[i], key) == 0) {
      return vals[i];
    }
  }
  return def;
}

static int geti_kv(const char *key, int def, char **keys, char **vals) {
  for (int i = 0; i < arrlen(keys); i++) {
    if (strcmp(keys[i], key) == 0) {
      return atoi(vals[i]);
    }
  }
  return def;
}

static void exe_cmd(struct FS_Project *project, const char *cmd, char **keys, char **vals) {
  if (strcmp(cmd, "Project") == 0) {
    // ignore
  } else if (strcmp(cmd, "Instrument") == 0) {
    const char *name = get_kv("Name", "Unknown", keys, vals);
    const char *exp = get_kv("Expansion", "", keys, vals);
    const char *vv = get_kv("Vrc6SawMasterVolume", "", keys, vals);
    struct FS_Instrument *instrument = fs_instrument_new(name);
    arrpush(project->instruments, instrument);
    if (strcmp(exp, "VRC6") == 0) {
      instrument->expansion = 1;
    }
    if (strcmp(vv, "Half") == 0) {
      instrument->vrc6saw = 1;
    }
  } else if (strcmp(cmd, "Envelope") == 0) {
    struct FS_Instrument *instrument = project->instruments[arrlen(project->instruments) - 1];
    const char *type = get_kv("Type", "", keys, vals);
    struct FS_Envelope *envelope = NULL;
    if (strcmp(type, "DutyCycle") == 0) {
      envelope = &instrument->dutycycle;
    } else if (strcmp(type, "Volume") == 0) {
      envelope = &instrument->volume;
    } else if (strcmp(type, "Pitch") == 0) {
      envelope = &instrument->pitch;
    }
    if (envelope) {
      envelope->loop = geti_kv("Loop", -1, keys, vals);
      envelope->release = geti_kv("Release", -1, keys, vals);
      const char *values = get_kv("Values", "", keys, vals);
      int chi = 0;
      int num = 0;
      int numi = 0;
      int sign = 1;
      while (values[chi]) {
        char ch = values[chi++];
        if (ch == '-') sign = -1;
        else if (ch >= '0' && ch <= '9') {
          num = (num * 10) + ch - '0';
          numi++;
        } else if (numi > 0) {
          arrpush(envelope->values, sign * num);
          num = 0;
          numi = 0;
          sign = 1;
        }
      }
      if (numi > 0) {
        arrpush(envelope->values, sign * num);
      }
    }
  } else if (strcmp(cmd, "Song") == 0) {
    const char *name = get_kv("Name", "Unknown", keys, vals);
    struct FS_Song *song = fs_song_new(name);
    arrpush(project->songs, song);
    song->length = geti_kv("Length", -1, keys, vals);
    song->looppoint = geti_kv("LoopPoint", -1, keys, vals);
    song->patternlength = geti_kv("PatternLength", -1, keys, vals);
    song->beatlength = geti_kv("BeatLength", -1, keys, vals);
    song->notelength = geti_kv("NoteLength", -1, keys, vals);
    const char *groove = get_kv("Groove", "", keys, vals);
    int chi = 0;
    int num = 0;
    int numi = 0;
    while (groove[chi]) {
      char ch = groove[chi++];
      if (ch >= '0' && ch <= '9') {
        num = (num * 10) + ch - '0';
        numi++;
      } else if (numi > 0) {
        arrpush(song->groove, num);
        num = 0;
        numi = 0;
      }
    }
    if (numi > 0) {
      arrpush(song->groove, num);
    }
  } else if (strcmp(cmd, "Channel") == 0) {
    struct FS_Song *song = project->songs[arrlen(project->songs) - 1];
    const char *type = get_kv("Type", "", keys, vals);
    if (strcmp(type, "Square1") == 0) {
      song->currentchannel = &song->square1;
      song->currentoctave = 1;
    } else if (strcmp(type, "Square2") == 0) {
      song->currentchannel = &song->square2;
      song->currentoctave = 1;
    } else if (strcmp(type, "Triangle") == 0) {
      song->currentchannel = &song->triangle;
      song->currentoctave = 0;
    } else if (strcmp(type, "Noise") == 0) {
      song->currentchannel = &song->noise;
      song->currentoctave = 1;
    } else if (strcmp(type, "VRC6Square1") == 0) {
      song->currentchannel = &song->vrc6square1;
      song->currentoctave = 1;
    } else if (strcmp(type, "VRC6Square2") == 0) {
      song->currentchannel = &song->vrc6square2;
      song->currentoctave = 1;
    } else if (strcmp(type, "VRC6Saw") == 0) {
      song->currentchannel = &song->vrc6saw;
      song->currentoctave = 1;
    }
  } else if (strcmp(cmd, "Pattern") == 0) {
    struct FS_Pattern *pattern = fs_pattern_new(get_kv("Name", "Unknown", keys, vals));
    struct FS_Song *song = project->songs[arrlen(project->songs) - 1];
    arrpush(song->currentchannel->patterns, pattern);
  } else if (strcmp(cmd, "Note") == 0) {
    struct FS_Song *song = project->songs[arrlen(project->songs) - 1];
    const char *valuestr = get_kv("Value", "", keys, vals);
    int value = parse_note(valuestr, song->currentoctave);
    if (value < 0) {
      fprintf(stderr, "Warning: Dropping note due to unknown value '%s'\n", valuestr);
    } else {
      struct FS_Song *song = project->songs[arrlen(project->songs) - 1];
      struct FS_Pattern *pattern =
        song->currentchannel->patterns[arrlen(song->currentchannel->patterns) - 1];
      struct FS_Note *note = fs_note_new(get_kv("Instrument", "", keys, vals));
      note->time = geti_kv("Time", -1, keys, vals);
      note->value = value;
      note->duration = geti_kv("Duration", -1, keys, vals);
      note->release = geti_kv("Release", -1, keys, vals);
      note->volume = geti_kv("Volume", -1, keys, vals);
      arrpush(pattern->notes, note);
    }
  } else if (strcmp(cmd, "PatternInstance") == 0) {
    struct FS_PatternInstance *instance =
      fs_patterninstance_new(get_kv("Pattern", "Unknown", keys, vals));
    instance->time = geti_kv("Time", -1, keys, vals);
    struct FS_Song *song = project->songs[arrlen(project->songs) - 1];
    arrpush(song->currentchannel->instances, instance);
  } else {
    fprintf(stderr, "Unknown command: '%s'\n", cmd);
    for (int i = 0; i < arrlen(keys); i++) {
      fprintf(stderr, "  '%s' = '%s'\n", keys[i], vals[i]);
    }
  }
}

static struct FS_Project *fs_project_load(const char *input) {
  struct FS_Project *project = fs_project_new();
  FILE *fp = fopen(input, "r");
  char line[2000];
  while (fgets(line, sizeof(line), fp) != NULL) {
    int state = 0;
    int chi = 0;
    char cmd[100];
    int cmdi = 0;
    char key[100];
    int keyi = 0;
    char val[1000];
    int vali = 0;
    char **keys = NULL;
    char **vals = NULL;
    while (line[chi]) {
      char ch = line[chi++];
      switch (state) {
        case 0: // start of line
          if (ch == '\t' || ch == ' ') {
          } else {
            cmd[0] = ch;
            cmdi = 1;
            state = 1;
          }
          break;
        case 1: // reading command
          if (ch == ' ') {
            cmd[cmdi] = 0;
            keyi = 0;
            state = 2;
          } else if (ch == '\n') {
            // empty command
            cmd[cmdi] = 0;
            exe_cmd(project, cmd, NULL, NULL);
            cmdi = 0;
            state = 0;
          } else {
            cmd[cmdi++] = ch;
          }
          break;
        case 2: // reading attribute
          if (ch == '=') {
            key[keyi] = 0;
            vali = 0;
            state = 3;
          } else if (ch == '\n') {
            // no more attributes
            state = 0;
          } else if (ch != ' ') {
            key[keyi++] = ch;
          }
          break;
        case 3: // reading value quote
          if (ch == '"') {
            state = 4;
          }
          break;
        case 4: // reading value
          if (ch == '"') {
            state = 5;
          } else {
            val[vali++] = ch;
          }
          break;
        case 5: // reading quote
          if (ch == '"') {
            // double quote
            val[vali++] = ch;
            state = 4;
          } else {
            // end quote
            val[vali] = 0;
            char *k = strdup(key);
            char *v = strdup(val);
            arrpush(keys, k);
            arrpush(vals, v);
            chi--;
            keyi = 0;
            state = 2;
          }
          break;
      }
    }
    if (cmdi > 0) {
      exe_cmd(project, cmd, keys, vals);
    }
    for (int i = 0; i < arrlen(keys); i++) {
      free(keys[i]);
      free(vals[i]);
    }
    arrfree(keys);
    arrfree(vals);
  }
  fclose(fp);

  // make sure notes and instances are sorted by time
  for (int s = 0; s < arrlen(project->songs); s++) {
    struct FS_Song *song = project->songs[s];
    fs_channel_sort(&song->square1);
    fs_channel_sort(&song->square2);
    fs_channel_sort(&song->triangle);
    fs_channel_sort(&song->noise);
    fs_channel_sort(&song->vrc6square1);
    fs_channel_sort(&song->vrc6square2);
    fs_channel_sort(&song->vrc6saw);
  }

  // clean up note duration, which should only be set if we need a note stop command
  for (int s = 0; s < arrlen(project->songs); s++) {
    struct FS_Song *song = project->songs[s];
    fs_channel_fixduration(&song->square1, song->patternlength, song->notelength);
    fs_channel_fixduration(&song->square2, song->patternlength, song->notelength);
    fs_channel_fixduration(&song->triangle, song->patternlength, song->notelength);
    fs_channel_fixduration(&song->noise, song->patternlength, song->notelength);
    fs_channel_fixduration(&song->vrc6square1, song->patternlength, song->notelength);
    fs_channel_fixduration(&song->vrc6square2, song->patternlength, song->notelength);
    fs_channel_fixduration(&song->vrc6saw, song->patternlength, song->notelength);
  }

  return project;
}

struct GV_Envelope {
  int loop;
  int release;
  int *values;
};

struct GV_Instrument {
  char *comment;
  char name[4];
  char wave[4];
  struct GV_Envelope volume;
  struct GV_Envelope pitch;
};

struct GV_Sequence {
  char *comment;
  char **patterns;
  int loop;
};

struct GV_Command {
  int time;
  char cmd[8];
};

struct GV_Channel {
  struct GV_Command **commands;
};

struct GV_Pattern {
  char *name;
  int length;
  struct GV_Channel **channels;
};

struct GV_Project {
  struct GV_Instrument **instruments;
  struct GV_Sequence **sequences;
  struct GV_Pattern **patterns;
};

static void gv_envelope_print(struct GV_Envelope *envelope, FILE *fp) {
  bool looped = false;
  for (int i = 0; i < arrlen(envelope->values); i++) {
    if (i == envelope->loop) {
      looped = true;
      fprintf(fp, "{");
    }
    fprintf(fp, "%d", envelope->values[i]);
    if (i == envelope->release) {
      looped = false;
      fprintf(fp, "}");
    }
    if (i < arrlen(envelope->values) - 1) {
      fprintf(fp, " ");
    }
  }
  if (looped) {
    fprintf(fp, "}");
  }
}

static struct GV_Instrument *gv_instrument_new(const char *comment) {
  struct GV_Instrument *instrument = calloc(1, sizeof(struct GV_Instrument));
  instrument->comment = strdup(comment);
  return instrument;
}

static void gv_instrument_print(struct GV_Instrument *instrument, FILE *fp) {
  fprintf(fp,
    "\n# %s\n"
    "new instrument %s\n"
    "  wave: %s\n"
    "  phase: continue\n",
    instrument->comment,
    instrument->name,
    instrument->wave
  );
  if (arrlen(instrument->volume.values) > 0) {
    fprintf(fp, "  volume: ");
    gv_envelope_print(&instrument->volume, fp);
    fprintf(fp, "\n");
  }
  if (arrlen(instrument->pitch.values) > 0) {
    fprintf(fp, "  pitch: ");
    gv_envelope_print(&instrument->pitch, fp);
    fprintf(fp, "\n");
  }
  fprintf(fp, "end\n");
}

static void gv_instrument_free(struct GV_Instrument *instrument) {
  free(instrument->comment);
  arrfree(instrument->volume.values);
  arrfree(instrument->pitch.values);
  free(instrument);
}

static struct GV_Sequence *gv_sequence_new(const char *comment) {
  struct GV_Sequence *sequence = calloc(1, sizeof(struct GV_Sequence));
  sequence->comment = strdup(comment);
  sequence->loop = -1;
  return sequence;
}

static void gv_sequence_print(struct GV_Sequence *sequence, int index, FILE *fp) {
  fprintf(fp,
    "\n# %s\n"
    "new sequence %d\n",
    sequence->comment,
    index
  );
  bool looped = false;
  for (int p = 0; p < arrlen(sequence->patterns); p++) {
    if (p == sequence->loop) {
      looped = true;
      fprintf(fp, "  {\n");
    }
    fprintf(fp, "  %s\n", sequence->patterns[p]);
  }
  if (looped) {
    fprintf(fp, "  }\n");
  }
  fprintf(fp, "end\n");
}

static void gv_sequence_free(struct GV_Sequence *sequence) {
  free(sequence->comment);
  for (int i = 0; i < arrlen(sequence->patterns); i++) {
    free(sequence->patterns[i]);
  }
  arrfree(sequence->patterns);
  free(sequence);
}

static struct GV_Command *gv_command_new() {
  struct GV_Command *cmd = calloc(1, sizeof(struct GV_Command));
  strcpy(cmd->cmd, "---:---");
  return cmd;
}

static struct GV_Channel *gv_channel_new() {
  return calloc(1, sizeof(struct GV_Channel));
}

static int gv_command_compare(const struct GV_Command **a, const struct GV_Command **b) {
  return (*a)->time - (*b)->time;
}

static void gv_channel_sort(struct GV_Channel *channel) {
  qsort(
    channel->commands,
    arrlen(channel->commands),
    sizeof(struct GV_Command *),
    (int (*)(const void *, const void *))gv_command_compare
  );
}

static void gv_channel_free(struct GV_Channel *channel) {
  for (int i = 0; i < arrlen(channel->commands); i++) {
    free(channel->commands[i]);
  }
  arrfree(channel->commands);
  free(channel);
}

static struct GV_Pattern *gv_pattern_new(const char *name) {
  struct GV_Pattern *pattern = calloc(1, sizeof(struct GV_Pattern));
  pattern->name = strdup(name);
  return pattern;
}

static void gv_pattern_print(struct GV_Pattern *pattern, FILE *fp) {
  fprintf(fp, "\nnew pattern %s\n", pattern->name);
  int time = -1;
  int *here = NULL;
  for (int ch = 0; ch < arrlen(pattern->channels); ch++) {
    struct GV_Channel *channel = pattern->channels[ch];
    gv_channel_sort(channel);
    if (arrlen(channel->commands) > 0) {
      // get time of first event
      if (channel->commands[0]->time < time || time < 0) {
        time = channel->commands[0]->time;
      }
    }
    arrpush(here, 0);
  }
  const char *beatstr = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  for (; time >= 0;) {
    int beat = time / 16;
    int step = time % 16;
    int nexttime = -1;
    if (beat >= 36) {
      fprintf(fp, "  #%X", step);
    } else {
      fprintf(fp, "  %c%X", beatstr[beat], step);
    }
    for (int ch = 0; ch < arrlen(pattern->channels); ch++) {
      struct GV_Channel *channel = pattern->channels[ch];
      int h = here[ch];
      if (h < arrlen(channel->commands) && channel->commands[h]->time == time) {
        fprintf(fp, "  %s", channel->commands[h]->cmd);
        here[ch] = h = h + 1;
      } else {
        fprintf(fp, "  ---:---");
      }
      if (h < arrlen(channel->commands) && (
        channel->commands[h]->time < nexttime || nexttime < 0
      )) {
        nexttime = channel->commands[h]->time;
      }
    }
    fprintf(fp, "\n");
    time = nexttime;
  }
  arrfree(here);

  // output END at pattern end
  fprintf(fp, "  %c%X", beatstr[pattern->length / 16], pattern->length % 16);
  for (int ch = 0; ch < arrlen(pattern->channels); ch++) {
    fprintf(fp, ch == 0 ? "  END:---" : "  ---:---");
  }
  fprintf(fp, "\nend\n");
}

static void gv_pattern_free(struct GV_Pattern *pattern) {
  free(pattern->name);
  for (int i = 0; i < arrlen(pattern->channels); i++) {
    gv_channel_free(pattern->channels[i]);
  }
  arrfree(pattern->channels);
  free(pattern);
}

static struct GV_Project *gv_project_new() {
  return calloc(1, sizeof(struct GV_Project));
}

static void gv_project_print(struct GV_Project *project, const char *input, FILE *fp) {
  fprintf(fp, "# Project converted from: %s\n", input);
  for (int i = 0; i < arrlen(project->instruments); i++) {
    gv_instrument_print(project->instruments[i], fp);
  }
  for (int i = 0; i < arrlen(project->sequences); i++) {
    gv_sequence_print(project->sequences[i], i, fp);
  }
  for (int i = 0; i < arrlen(project->patterns); i++) {
    gv_pattern_print(project->patterns[i], fp);
  }
}

static void gv_project_free(struct GV_Project *project) {
  for (int i = 0; i < arrlen(project->instruments); i++) {
    gv_instrument_free(project->instruments[i]);
  }
  arrfree(project->instruments);
  for (int i = 0; i < arrlen(project->sequences); i++) {
    gv_sequence_free(project->sequences[i]);
  }
  arrfree(project->sequences);
  for (int i = 0; i < arrlen(project->patterns); i++) {
    gv_pattern_free(project->patterns[i]);
  }
  arrfree(project->patterns);
  free(project);
}

enum FS_ChannelType {
  FS_SQUARE,
  FS_TRIANGLE,
  FS_NOISE,
  FS_VRC6SQUARE,
  FS_VRC6SAW
};

static void print_instrument(int inst, char *out) {
  const char *s = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";
  out[0] = 'I';
  int i = inst / 10;
  if (i >= strlen(s)) {
    out[1] = 'X';
    out[2] = 'X';
  } else {
    out[1] = s[i];
    out[2] = '0' + (inst % 10);
  }
}

static const char *channel_type_str(enum FS_ChannelType type) {
  switch (type) {
    case FS_SQUARE:     return "square";
    case FS_TRIANGLE:   return "triangle";
    case FS_NOISE:      return "noise";
    case FS_VRC6SQUARE: return "VRC6 square";
    case FS_VRC6SAW:    return "VRC6 saw";
  }
  return "unknown";
}

static int find_or_add_instrument(
  struct GV_Project *gv,
  struct FS_Project *fs,
  const char *instname,
  enum FS_ChannelType type
) {
  for (int i = 0; i < arrlen(fs->instruments); i++) {
    struct FS_Instrument *instrument = fs->instruments[i];
    if (strcmp(instrument->name, instname) == 0) {
      // create instrument
      char name[100];
      snprintf(name, sizeof(name), "%s [%s]", instname, channel_type_str(type));
      int gvi = 0;
      for (; gvi < arrlen(gv->instruments); gvi++) {
        struct GV_Instrument *gvinst = gv->instruments[gvi];
        if (strcmp(name, gvinst->comment) == 0) {
          break;
        }
      }
      int result;
      if (gvi < arrlen(gv->instruments)) {
        result = gvi + 1;
      } else {
        struct GV_Instrument *gvinst = gv_instrument_new(name);
        switch (type) {
          case FS_SQUARE:
            if (arrlen(instrument->dutycycle.values) <= 0) {
              strcpy(gvinst->wave, "sq2");
            } else {
              int d = instrument->dutycycle.values[0];
              if (d == 1 || d == 3) {
                strcpy(gvinst->wave, "sq4");
              } else if (instrument->dutycycle.values[0] == 2) {
                strcpy(gvinst->wave, "sq8");
              } else {
                strcpy(gvinst->wave, "sq2");
              }
            }
            break;
          case FS_TRIANGLE:
            strcpy(gvinst->wave, "tri");
            break;
          case FS_NOISE:
            strcpy(gvinst->wave, "rnd");
            break;
          case FS_VRC6SQUARE:
            if (arrlen(instrument->dutycycle.values) <= 0) {
              strcpy(gvinst->wave, "sq1");
            } else {
              int d = instrument->dutycycle.values[0];
              gvinst->wave[0] = 's';
              gvinst->wave[1] = 'q';
              gvinst->wave[2] = '1' + d;
            }
            break;
          case FS_VRC6SAW:
            strcpy(gvinst->wave, "saw");
            break;
        }
        #define COPY_ENV(id)  do {                                     \
            gvinst->id.loop = instrument->id.loop;                     \
            gvinst->id.release = instrument->id.release;               \
            for (int i = 0; i < arrlen(instrument->id.values); i++) {  \
              arrpush(gvinst->id.values, instrument->id.values[i]);    \
            }                                                          \
          } while (0)
        COPY_ENV(volume);
        COPY_ENV(pitch);
        #undef COPY_ENV
        arrpush(gv->instruments, gvinst);
        result = arrlen(gv->instruments);
        print_instrument(result, gvinst->name);
      }
      return result;
    }
  }
  fprintf(stderr, "WARING: Missing instrument '%s'\n", instname);
  return -1;
}

static void gv_channel_convert(
  struct GV_Project *gv,
  struct GV_Pattern **patterns,
  struct FS_Project *fs,
  struct FS_Song *song,
  struct FS_Channel *fschannel,
  enum FS_ChannelType type
) {
  for (int p = 0; p < arrlen(patterns); p++) {
    struct GV_Pattern *pattern = patterns[p];
    // TODO: support different pattern lengths instead of song default
    pattern->length = song->patternlength * song->beatlength;
    struct GV_Channel *channel = gv_channel_new();
    arrpush(pattern->channels, channel);
  }
  for (int ins = 0; ins < arrlen(fschannel->instances); ins++) {
    struct FS_PatternInstance *instance = fschannel->instances[ins];
    struct FS_Pattern *fspattern = NULL;
    for (int p = 0; p < arrlen(fschannel->patterns); p++) {
      if (strcmp(fschannel->patterns[p]->name, instance->pattern) == 0) {
        fspattern = fschannel->patterns[p];
        break;
      }
    }
    if (!fspattern) {
      fprintf(stderr, "WARNING: Missing pattern '%s'\n", instance->pattern);
      continue;
    }
    struct GV_Pattern *pattern = patterns[instance->time];
    struct GV_Channel *channel = pattern->channels[arrlen(pattern->channels) - 1];
    int inst = -1;
    for (int n = 0; n < arrlen(fspattern->notes); n++) {
      struct FS_Note *fsnote = fspattern->notes[n];
      struct GV_Command *cmd = gv_command_new();
      arrpush(channel->commands, cmd);
      cmd->time = fsnote->time * song->beatlength / song->notelength;
      print_note(fsnote->value, cmd->cmd);
      int ninst = find_or_add_instrument(gv, fs, fsnote->instrument, type);
      if (ninst != inst && ninst >= 0) {
        inst = ninst;
        print_instrument(inst, &cmd->cmd[4]);
      }
      if (fsnote->release > 0) {
        struct GV_Command *rel = gv_command_new();
        arrpush(channel->commands, rel);
        rel->time = (fsnote->time + fsnote->release) * song->beatlength / song->notelength;
        rel->cmd[0] = 'O';
        rel->cmd[1] = 'F';
        rel->cmd[2] = 'F';
      }
      if (fsnote->duration > 0) {
        struct GV_Command *stop = gv_command_new();
        arrpush(channel->commands, stop);
        stop->time = (fsnote->time + fsnote->duration) * song->beatlength / song->notelength;
        stop->cmd[0] = '0';
        stop->cmd[1] = '0';
        stop->cmd[2] = '0';
      }
    }
  }
}

int famistudio_main(int argc, const char **argv) {
  if (argc != 2) {
    famistudio_help();
    fprintf(stderr, "\nExpecting: famistudio <input.txt> <output.txt>\n");
    return 1;
  }
  const char *input = argv[0];
  const char *output = argv[1];
  struct FS_Project *fs = fs_project_load(input);
  struct GV_Project *gv = gv_project_new();

  fs_project_print(fs);

  // convert each song
  for (int s = 0; s < arrlen(fs->songs); s++) {
    struct FS_Song *song = fs->songs[s];
    struct GV_Sequence *seq = gv_sequence_new(song->name);
    char name[100];
    snprintf(name, sizeof(name), "s%02dsetup", s);
    arrpush(seq->patterns, strdup(name));
    arrpush(gv->sequences, seq);
    seq->loop = song->looppoint + 1; // skip over setup

    struct GV_Pattern **songpats = NULL;
    for (int p = 0; p < song->length; p++) {
      snprintf(name, sizeof(name), "s%02dp%03d", s, p);
      struct GV_Pattern *pattern = gv_pattern_new(name);
      arrpush(gv->patterns, pattern);
      arrpush(songpats, pattern);
      arrpush(seq->patterns, strdup(name));
    }
    int chanlen = 0;
    if (arrlen(song->square1.instances) > 0) {
      chanlen++;
      gv_channel_convert(gv, songpats, fs, song, &song->square1, FS_SQUARE);
    }
    if (arrlen(song->square2.instances) > 0) {
      chanlen++;
      gv_channel_convert(gv, songpats, fs, song, &song->square2, FS_SQUARE);
    }
    if (arrlen(song->triangle.instances) > 0) {
      chanlen++;
      gv_channel_convert(gv, songpats, fs, song, &song->triangle, FS_TRIANGLE);
    }
    if (arrlen(song->noise.instances) > 0) {
      chanlen++;
      gv_channel_convert(gv, songpats, fs, song, &song->noise, FS_NOISE);
    }
    if (arrlen(song->vrc6square1.instances) > 0) {
      chanlen++;
      gv_channel_convert(gv, songpats, fs, song, &song->vrc6square1, FS_VRC6SQUARE);
    }
    if (arrlen(song->vrc6square2.instances) > 0) {
      chanlen++;
      gv_channel_convert(gv, songpats, fs, song, &song->vrc6square2, FS_VRC6SQUARE);
    }
    if (arrlen(song->vrc6saw.instances) > 0) {
      chanlen++;
      gv_channel_convert(gv, songpats, fs, song, &song->vrc6saw, FS_VRC6SAW);
    }
    arrfree(songpats);

    { // song setup
      snprintf(name, sizeof(name), "s%02dsetup", s);
      struct GV_Pattern *pattern = gv_pattern_new(name);
      for (int ch = 0; ch < chanlen; ch++) {
        struct GV_Channel *channel = gv_channel_new();
        arrpush(pattern->channels, channel);
        if (ch == 0) {
          int tempo = fs_song_tempo(song);
          struct GV_Command *cmd = gv_command_new();
          cmd->time = 0;
          cmd->cmd[4] = '0' + (tempo / 100);
          cmd->cmd[5] = '0' + ((tempo / 10) % 10);
          cmd->cmd[6] = '0' + (tempo % 10);
          arrpush(channel->commands, cmd);
        }
      }
      arrpush(gv->patterns, pattern);
    }
  }

  FILE *fp = fopen(output, "w");
  if (!fp) {
    fprintf(stderr, "Error: Failed to write to: %s\n", output);
    fs_project_free(fs);
    gv_project_free(gv);
    return 1;
  }
  gv_project_print(gv, input, fp);
  fclose(fp);

  fs_project_free(fs);
  gv_project_free(gv);

  return 0;
}
