// $Id: eupplayer.cc,v 1.9 2000/04/12 23:17:18 hayasaka Exp $

/*      Artistic Style
 *
 * ./astyle --style=stroustrup --convert-tabs --add-braces eupplayer.cpp
 */

#include <cassert>
#include <iostream>
#include <cstring>
#include <cmath>
#include <sys/stat.h>
//#include <unistd.h>
#if defined ( __MINGW32__ )
#include <_bsd_types.h>
#endif
#include "eupplayer.hpp"
#include "sintbl.hpp"

/* 0 は 0
   1 は heplay っぽく
   2 は 純正っぽく */
#define OVERSTEP 2

#define DB(a)

#define DB_PROCESSING(mes) DB(("* com=%02x tr=%02x sl=%02x sh=%02x p4=%02x p5=%02x (%s)\n", \
  (pl->_curP)[0], (pl->_curP)[1], (pl->_curP)[2], \
  (pl->_curP)[3], (pl->_curP)[4], (pl->_curP)[5], mes))

#if OVERSTEP == 2
#define COMPRESSOVERSTEPS if (stepTime >= 384) stepTime = 383
/* .... >= 384 ってのは steptime >= 小節長 のつもり。
    HEat で作られた .eup ファイルの中に存在することがある。  */
#else
#define COMPRESSOVERSTEPS
#endif

#define WAIT4NEXTSTEP \
  int stepTime = (pl->_curP)[2] + 0x80*(pl->_curP)[3]; \
  COMPRESSOVERSTEPS; \
  if (stepTime > pl->_stepTime) return 1;

int EUPPlayer_cmd_8x(int cmd, EUPPlayer *pl)
{
    /* ノートオフ */
    DB_PROCESSING("Note off");

    /* 単独で来るはずが無いからエラーにする.  でも, DATA CONTINUE の直後
        なら有り得るかな?  */

    return EUPPlayer_cmd_INVALID(cmd, pl);
}

int EUPPlayer_cmd_9x(int cmd, EUPPlayer *pl)
{
    /* ノートオン */
    WAIT4NEXTSTEP;
    DB_PROCESSING("Note on");

    if (((pl->_curP)[6] & 0xf0) != 0x80) {
        /* 次のコマンドが不正だってメッセージ出さなきゃなぁ  */
        return EUPPlayer_cmd_INVALID(cmd, pl);
    }

    if ((cmd & 0x0f) != 0) {
        DB(("MIDI-ch is not zero (%02x).\n", cmd));    // pb_theme など
    }

    int track = (pl->_curP)[1];
    int note = (pl->_curP)[4];
    int onVelo = (pl->_curP)[5];
    int gateTime = (0x1*(pl->_curP[7]) + 0x10*(pl->_curP[8]) +
                    0x100*(pl->_curP[9]) + 0x1000*(pl->_curP[10]));
    int offVelo = (pl->_curP)[11];
    //DB(("tr %02x\n", track));
    //DB(("step=%d, track=%d, note=%d, on=%d, gate=%d, off=%d\n", stepTime, track, note, onVelo, gateTime, offVelo));
    if (offVelo == 0 || offVelo >= 0x80) {
        offVelo = onVelo;    // これでいいのだろうか?  (灰p.437)
    }
    pl->_outputDev->note(pl->_track2channel[track], note, onVelo, offVelo, gateTime);

    pl->_curP += 12;
    return 0;
}

int EUPPlayer_cmd_ax(int cmd, EUPPlayer *pl)
{
    /* ポリフォニックアフタータッチ */
    DB_PROCESSING("Polyphonic after touch");
    return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
}

int EUPPlayer_cmd_bx(int cmd, EUPPlayer *pl)
{
    /* コントロールチェンジ */
    WAIT4NEXTSTEP;
    DB_PROCESSING("Control change");
    int track = (pl->_curP)[1];
    int control = (pl->_curP)[4];
    int value = (pl->_curP)[5];
    pl->_outputDev->controlChange(pl->_track2channel[track], control, value);
    pl->_curP += 6;
    return 0;
}

int EUPPlayer_cmd_cx(int cmd, EUPPlayer *pl)
{
    /* プログラムチェンジ */
    WAIT4NEXTSTEP;
    DB_PROCESSING("Program change");
    int track = (pl->_curP)[1];
    int num = (pl->_curP)[4];
    pl->_outputDev->programChange(pl->_track2channel[track], num);
    pl->_curP += 6;
    return 0;
}

int EUPPlayer_cmd_dx(int cmd, EUPPlayer *pl)
{
    /* チャンネルアフタータッチ */
    DB_PROCESSING("Channel after touch");
    return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
}

int EUPPlayer_cmd_ex(int cmd, EUPPlayer *pl)
{
    /* ピッチベンド */
    /*
  6.9 Pitch bend.


    ■ U: Pitch bend

    Form 1:
    ┌───────────────────────────────┐
    │u<bend>                        │
    └───────────────────────────────┘
    Parameter.
        <Bend> -8192 to +8191

    Description.
    This command allows subtle changes in pitch.
    For the built-in sound source, moving the pitch bend fully (-8192 to +8191) changes the pitch by two octaves.

    Format 2:
    ┌───────────────────────────────┐
    │u%<ratio>[,<pitch difference>] │
    └───────────────────────────────┘
    [Parameters].
        <Ratio> -10000 to +10000 (in percentages)
        <Pitch difference> -96 to +96 (semitone units)

    [Explanation]
    The pitch bend is determined by the ratio of the pitch bend width between pitches.
    The pitch bend width between each interval must be preset by the note assignment in the control line.
    If <Ratio> < 0>, the pitch bend width must be "<Pitch Difference> > 0"; if <Ratio> > 0, the pitch bend width must be "<Pitch Difference> < 0".
    */
    WAIT4NEXTSTEP;
    DB_PROCESSING("Pitch bend");

    int track = (pl->_curP)[1];
    int value = (pl->_curP)[4] + ((pl->_curP)[5] << 7);
    pl->_outputDev->pitchBend(pl->_track2channel[track], value);

    pl->_curP += 6;
    return 0;
}

int EUPPlayer_cmd_f0(int cmd, EUPPlayer *pl)
{
    /* エクスクルーシブステータス */
    DB_PROCESSING("Exclusive status");
    //return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
    int SysEx_flag = 1;
    while (1 == SysEx_flag) {
//         printf("0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x 0x%.2x", \
//             (pl->_curP)[0], \
//             (pl->_curP)[1], \
//             (pl->_curP)[2], \
//             (pl->_curP)[3], \
//             (pl->_curP)[4], \
//             (pl->_curP)[5]);
        if ((0xf7 == (pl->_curP)[1]) \
            || (0xf7 == (pl->_curP)[2]) \
            || (0xf7 == (pl->_curP)[3]) \
            || (0xf7 == (pl->_curP)[4]) \
            || (0xf7 == (pl->_curP)[5])) {
            if (0xf0 == (pl->_curP)[0]) {
//                 printf(" 11110000 System Common Msg System Exclusive Status with $F7 End of Exclusive.");
            }
            else {
//                 printf(" XXXXXXXX System Common Msg System Exclusive Data with $F7 End of Exclusive.");
            }
            SysEx_flag = 0;
        }
        else if ((0xf7 == (pl->_curP)[0]) \
            && (0xff == (pl->_curP)[1]) \
            && (0xff == (pl->_curP)[2]) \
            && (0xff == (pl->_curP)[3]) \
            && (0xff == (pl->_curP)[4]) \
            && (0xff == (pl->_curP)[5])) {
//             printf(" 11110111 System Common Msg System Exclusive End of Exclusive.");
            SysEx_flag = 0;
        }
        else {
            if (0xf0 == (pl->_curP)[0]) {
//                 printf(" 11110000 System Common Msg System Exclusive Status.");
            }
            else {
//                 printf(" XXXXXXXX System Common Msg System Exclusive Data.");
            }
        }
//         printf("\n");
//         std::fflush(stdout);
        pl->_curP += 6;
    }
    return 0;
}

int EUPPlayer_cmd_f1(int cmd, EUPPlayer *pl)
{
    /* 未定義 */
    DB_PROCESSING("Undefined");
    return EUPPlayer_cmd_INVALID(cmd, pl);
}

int EUPPlayer_cmd_f2(int cmd, EUPPlayer *pl)
{
    /* 小節マーカー */
    WAIT4NEXTSTEP;
    DB_PROCESSING("Bar");

#if OVERSTEP == 0 || OVERSTEP == 2
    pl->_stepTime = 0;
    // 仕様通りだとこうするべきなのだと思う
    // ただし、純正プレイヤーではさらに変なことをやってるっぽい。
#endif
#if OVERSTEP == 1
    pl->_stepTime -= stepTime;
    // こうすると heplay っぽい
#endif
    pl->_curP += 6;
    return 0;
}

int EUPPlayer_cmd_f3(int cmd, EUPPlayer *pl)
{
    /* 未定義 */
    DB_PROCESSING("Undefined");
    return EUPPlayer_cmd_INVALID(cmd, pl);
}

int EUPPlayer_cmd_f4(int cmd, EUPPlayer *pl)
{
    /* 未定義 */
    DB_PROCESSING("Undefined");
    return EUPPlayer_cmd_INVALID(cmd, pl);
}

int EUPPlayer_cmd_f5(int cmd, EUPPlayer *pl)
{
    /* 未定義 */
    DB_PROCESSING("Undefined");
    return EUPPlayer_cmd_INVALID(cmd, pl);
}

int EUPPlayer_cmd_f6(int cmd, EUPPlayer *pl)
{
    /* 未定義 */
    DB_PROCESSING("Undefined");
    return EUPPlayer_cmd_INVALID(cmd, pl);
}

int EUPPlayer_cmd_f7(int cmd, EUPPlayer *pl)
{
    /* END OF エクスクルーシブ */
    DB_PROCESSING("End of exclusive");
    return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
}

int EUPPlayer_cmd_f8(int cmd, EUPPlayer *pl)
{
    /* テンポ */
    WAIT4NEXTSTEP;
    DB_PROCESSING("Tempo");
    int t = 30 + (pl->_curP)[4] + ((pl->_curP)[5] << 7);
    pl->tempo(t);
    (pl->_curP) += 6;
    return 0;
}

int EUPPlayer_cmd_f9(int cmd, EUPPlayer *pl)
{
    /* 未定義 */
    DB_PROCESSING("Undefined");
    return EUPPlayer_cmd_INVALID(cmd, pl);
}

int EUPPlayer_cmd_fa(int cmd, EUPPlayer *pl)
{
    /* USER CALL PROGRAM */
    DB_PROCESSING("User call program");
    return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
}

int EUPPlayer_cmd_fb(int cmd, EUPPlayer *pl)
{
    /* パターン番号 */
    DB_PROCESSING("Pattern number");
    return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
}

int EUPPlayer_cmd_fc(int cmd, EUPPlayer *pl)
{
    /* TRACK COMMAND */
    DB_PROCESSING("Track command");
    return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
}

int EUPPlayer_cmd_fd(int cmd, EUPPlayer *pl)
{
    /* DATA CONTINUE */
    WAIT4NEXTSTEP;
    DB_PROCESSING("Data continue");
    pl->stopPlaying(); /* 本来は一時停止 */
    return EUPPlayer_cmd_NOTSUPPORTED(cmd, pl);
}

int EUPPlayer_cmd_fe(int cmd, EUPPlayer *pl)
{
    /* 終端マーカー */
    WAIT4NEXTSTEP;
    DB_PROCESSING("End mark");
    DB(("EUPPlayer: playing terminated.\n"));
    pl->stopPlaying();
    return 1;
}

int EUPPlayer_cmd_ff(int cmd, EUPPlayer *pl)
{
    /* ダミーコード */
    DB_PROCESSING("Dummy");
    pl->_curP += 6;
    return 0;
}

typedef int (*CommandProc)(int, EUPPlayer *);

static CommandProc const _fCommands[0x10] = {
    EUPPlayer_cmd_f0, EUPPlayer_cmd_f1, EUPPlayer_cmd_f2, EUPPlayer_cmd_f3,
    EUPPlayer_cmd_f4, EUPPlayer_cmd_f5, EUPPlayer_cmd_f6, EUPPlayer_cmd_f7,
    EUPPlayer_cmd_f8, EUPPlayer_cmd_f9, EUPPlayer_cmd_fa, EUPPlayer_cmd_fb,
    EUPPlayer_cmd_fc, EUPPlayer_cmd_fd, EUPPlayer_cmd_fe, EUPPlayer_cmd_ff,
};

int EUPPlayer_cmd_fx(int cmd, EUPPlayer *pl)
{
    return (_fCommands[cmd & 0x0f])(cmd, pl);
}

/* EUPPlayer */

EUPPlayer::EUPPlayer()
{
    for (int i = 0; i < _maxTrackNum; i++) {
        _track2channel[i] = 0;
    }

    _outputDev = nullptr;
    _stepTime = 0;
    _tempo = -1;
    this->stopPlaying();
}

EUPPlayer::~EUPPlayer()
{
    this->stopPlaying();
}

void EUPPlayer::outputDevice(PolyphonicAudioDevice *outputDev)
{
    this->stopPlaying();
    _outputDev = outputDev;
    this->tempo(this->tempo());
}

void EUPPlayer::mapTrack_toChannel(int track, int channel)
{
    assert(track >= 0);
    assert(track < _maxTrackNum);
    assert(channel >= 0);
    //assert(channel < _maxChannelNum);

    _track2channel[track] = channel;
}

void EUPPlayer::startPlaying(uint8_t const *ptr)
{
    _isPlaying = 0;
    if (ptr != nullptr) {
        _curP = ptr;
        _isPlaying = 1;
        _stepTime = 0;
    }

#if 0
    for (int track = 0; track < _maxTrackNum; track++) {
        _outputDev->programChange(_track2channel[track], 0);
    }
#endif
}

void EUPPlayer::stopPlaying()
{
    _isPlaying = 0;
}

bool EUPPlayer::isPlaying() const
{
    return (_isPlaying) ? true : false;
}

static CommandProc const _commands[0x08] = {
  EUPPlayer_cmd_8x, EUPPlayer_cmd_9x, EUPPlayer_cmd_ax, EUPPlayer_cmd_bx,
  EUPPlayer_cmd_cx, EUPPlayer_cmd_dx, EUPPlayer_cmd_ex, EUPPlayer_cmd_fx,
};

int EUPPlayer_cmd_INVALID(int cmd, EUPPlayer *pl)
{
//     std::cerr << "EUPPlayer: invalid command '" << std::hex << cmd << "'.\n";
    DB(("EUPPlayer: invalid command '%02x'.\n", cmd));
    (pl->_curP) += 6;
    return 1;
}

int EUPPlayer_cmd_NOTSUPPORTED(int cmd, EUPPlayer *pl)
{
//     std::cerr << "EUPPlayer: not supported command '" << std::hex << cmd << "'.\n";
    DB(("EUPPlayer: not supported command '%02x'.\n", cmd));
    (pl->_curP) += 6;
    return 1;
}

void EUPPlayer::nextTick()
{
    // steptime ひとつ分進める

    if (this->isPlaying())
        for (;;) {
            int cmd = *_curP;
            CommandProc cmdProc = EUPPlayer_cmd_INVALID;
            if (cmd >= 0x80) {
                cmdProc = _commands[((cmd-0x80)>>4) & 0x07];
            }
            if (cmdProc(cmd, this)) {
                break;
            }
        }

    if (_outputDev != nullptr) {
        _outputDev->nextTick();
    }

    _stepTime++;
}

int EUPPlayer::tempo() const
{
    return _tempo;
}

void EUPPlayer::tempo(int t)
{
    _tempo = t;

    if (_tempo <= 0) {
        return;
    }

    int t0 = 96 * t;
    struct timeval tv{};
    tv.tv_sec = 60 / t0;
    tv.tv_usec = ((60*1000*1000) / t0) - tv.tv_sec*1000*1000;
    if (_outputDev != nullptr) {
        _outputDev->timeStep(tv);
    }
}
