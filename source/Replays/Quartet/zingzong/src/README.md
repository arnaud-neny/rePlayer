### Zingzong command-line player

## Usage

    zingzong [OPTIONS] <song.4v> [<inst.set>]
    zingzong [OPTIONS] <music.4q>

### Options

 |Short| Long option    | Description                                        |
 |-----|----------------|----------------------------------------------------|
 |  -h | --help --usage | Print this message and exit.                       |
 |  -V | --version      | Print version and copyright and exit.              |
 |  -t | --tick=HZ      | Set player tick rate (default is 200hz).           |
 |  -r | --rate=[R,]HZ  | Set re-sampling method and rate (qerp,48K).        |
 |  -l | --length=TIME  | Set play time.                                     |
 |  -b | --blend=[X,]Y  | Set channel mapping and blending (see below).      |
 |  -m | --mute=CHANS   | Mute selected channels (bit-field or string).      |
 |  -i | --ignore=CHANS | Ignore selected channels (bit-field or string).    |
 |  -o | --output=URI   | Set output file name (`-w` or `-c`).               |
 |  -c | --stdout       | Output raw PCM to stdout or file (native 16-bit).  |
 |  -n | --null         | Output to the void.                                |
 |  -w | --wav          | Generated a .wav file (implicit if output is set). |


### Time

If time is not set the player tries to auto detect the music duration.
However a number of musics are going into unnecessary loops which
makes it hard to properly detect.

If time is set to zero `0` or `inf` the player will run forever.

 * 0 or `inf` represents an infinite duration
 * comma `,` dot `.` or double-quote `"` separates milliseconds
 * `m` or quote to suffix minutes
 * `h` to suffix hour


### Blending

Blending defines how the 4 channels are mapped to stereo output.
The 4 channels are mapped as left pair (lP) and right pair (rP).

 `X` | `lP`  | `rP`  |
 ----|-------|-------|
 `B` | `A+B` | `C+D` |
 `C` | `A+C` | `B+D` |
 `D` | `A+D` | `B+C` |
 
Both channels pairs are blended together according to `Y`.

  `Y`  |  Left output  |   Right output  |
 ------|---------------|-----------------|
 `0`   | `100%lP`      | `100%rP`        |
 `64`  | `75%lP+25%rP` | `R=25%lP+75%rP` |
 `128` | `50%lP+50%rP` | `R=50%lP+50%rP` |
 `192` | `25%lP+75%rP` | `R=75%lP+25%rP` |
 `256` | `100%rP`      | `R=100%lP`      |

 `L:=((256-Y).lP+Y.rP)/256   R:=(Y.lP+(256-Y).rP)/256`


### Output

Options `-n/--null`,`-c/--stdout` and `-w/--wav` are used to set the
output type. The last one is used. Without it the default output type
is used which should be playing sound via the default or configured
libao driver.

The `-o/--output` option specify the output depending on the output
type.

 * `-n/--null` output is ignored.
 * `-c/--stdout` output to the specified file instead of `stdout`.
 * `-w/--wav` unless set output is a file based on song filename.


### Channel selection

 Select channels to be either muted or ignored. It can be either:
 
 * an integer representing a mask of selected channels (C-style prefix).
 * a string containing the letter A to D (case insensitive) in any order.
