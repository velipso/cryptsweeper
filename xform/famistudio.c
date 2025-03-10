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
  struct FS_Envelope volume;
  struct FS_Envelope dutycycle;
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
  printf("    Volume Envelope: ");
  fs_envelope_print(&instrument->volume);
  printf("    Duty Cycle Envelope: ");
  fs_envelope_print(&instrument->dutycycle);
}

static void fs_instrument_free(struct FS_Instrument *instrument) {
  free(instrument->name);
  arrfree(instrument->volume.values);
  arrfree(instrument->dutycycle.values);
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
  return song;
}

static void fs_song_print(struct FS_Song *song) {
  printf(
    "  Song '%s' Length(%d) LoopPoint(%d) PatternLength(%d) NoteLength(%d) "
    "Groove: {",
    song->name,
    song->length,
    song->looppoint,
    song->patternlength,
    song->notelength
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
    if (strcmp(type, "Volume") == 0) {
      envelope = &instrument->volume;
    } else if (strcmp(type, "DutyCycle") == 0) {
      envelope = &instrument->dutycycle;
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
    } else if (strcmp(type, "Square2") == 0) {
      song->currentchannel = &song->square2;
    } else if (strcmp(type, "Triangle") == 0) {
      song->currentchannel = &song->triangle;
    } else if (strcmp(type, "Noise") == 0) {
      song->currentchannel = &song->noise;
    } else if (strcmp(type, "VRC6Square1") == 0) {
      song->currentchannel = &song->vrc6square1;
    } else if (strcmp(type, "VRC6Square2") == 0) {
      song->currentchannel = &song->vrc6square2;
    } else if (strcmp(type, "VRC6Saw") == 0) {
      song->currentchannel = &song->vrc6saw;
    }
  } else if (strcmp(cmd, "Pattern") == 0) {
    struct FS_Pattern *pattern = fs_pattern_new(get_kv("Name", "Unknown", keys, vals));
    struct FS_Song *song = project->songs[arrlen(project->songs) - 1];
    arrpush(song->currentchannel->patterns, pattern);
  } else if (strcmp(cmd, "Note") == 0) {
    const char *valuestr = get_kv("Value", "", keys, vals);
    int value = parse_note(valuestr, 0);
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
  return project;
}

int famistudio_main(int argc, const char **argv) {
  if (argc != 2) {
    famistudio_help();
    fprintf(stderr, "\nExpecting: famistudio <input.txt> <output.txt>\n");
    return 1;
  }
  struct FS_Project *project = fs_project_load(argv[0]);
  const char *output = argv[1];

  // everything loaded into project
  fs_project_print(project);

  // free project
  fs_project_free(project);

  return 0;
}
