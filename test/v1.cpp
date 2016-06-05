#include <rlibrary/rstdio.h>
#include <cstdlib>
#include <algorithm>
#include <vector>
#include <ctime>
#include <map>
using namespace std;
using namespace rtypes;

// pretend to be a pokgame version

/* create a binary stream to output information to the game engine */
static standard_binary_stream binstdout;
static map<std::string,uint32> netobjs;

static void create_handles();
static void send_tiles();
static void send_first_map();

int main(int,const char* argv[])
{
    errConsole << argv[0] << ": test version program started" << endline;
    srand(time(NULL));

    // create handles for network objects
    create_handles();

    // do greetings
    binstdout << binary_string_null_terminated;
    binstdout << "pokgame-greetings" << "pokgame-binary" << "QuirkyLands"
           << "85hd72jdmx91jswq";

    // send static network objects
    binstdout << byte(0003) << ushort(32) << ushort(11)
              << ushort(6) << ushort(1) << ushort(2)
              << ushort(0) << ushort(0);
    send_tiles();

    // write netobj id for player character
    binstdout << netobjs["player"];

    // write netobj id for world; then write first map
    binstdout << netobjs["world"];
    send_first_map();
    binstdout.flush_output();

    str trash(128);
    auto& device = binstdout.get_device();
    while (true) {
        device.read(trash);
        if (device.get_last_operation_status() == no_input) {
            errConsole << argv[0] << ": Received end of file; shutting down" << endline;
            break;
        }
        if (device.get_last_operation_status() != success_read) {
            errConsole << argv[0] << ": Could not read any more from peer" << endline;
            break;
        }
        errConsole << argv[0] << ": Read " << trash.size() << " bytes" << endline;
    }
}

void create_handles()
{
    netobjs["player"] = 33;
    netobjs["world"] = 44;
    netobjs["mapA"] = 55;
    netobjs["mapA.1"] = 56;
}

void send_tiles()
{
    auto random_byte = []{return byte(rand()%0xff);};
    binstdout << ushort(1) // number of tiles
           << ushort(0); // impassable tile cutoff

    // now send the source image
    binstdout << byte(0); // flag RGB image
    for (int i = 0;i < 32;++i)
        for (int j = 0;j < 32;++j)
            for (int k = 0;k < 3;++k)
                binstdout << random_byte();

    binstdout << ushort(0); // no animation data
    for (int i = 0;i < 10;++i)
        binstdout << ushort(0); // no terrain info
}

static void gen_maze_map(vector<vector<int> >& tiles,const pair<int,int>& pos)
{
    static const pair<int,int> OFFSETS[] = {make_pair(0,-1),make_pair(-1,0),make_pair(1,0),make_pair(0,1)};
    vector<pair<int,int> > positions;

    // mark unvisited adjacencies as impassable
    for (int i = 0;i < 4;++i) {
        int px, py;
        px = OFFSETS[i].first + pos.first;
        py = OFFSETS[i].second + pos.second;
        if (px < 0 || px >= 20 || py < 0 || py >= 20)
            continue;
        if (tiles[px][py] == -1) {
            positions.push_back( make_pair(px,py) );
            tiles[px][py] = 0;
        }
    }

    // randomly shuffle the positions and recursively compute paths to make the
    // maze
    if (positions.size() > 0) {
        tiles[pos.first][pos.second] = 1; // mark current position as passable
        random_shuffle(positions.begin(),positions.end());
        for (auto& p : positions) {
            gen_maze_map(tiles,p);
        }
    }
}

void send_first_map()
{
    // send map header
    binstdout << netobjs["mapA"] // netobj id
              << ushort(0) // map flags
              << uint32(1) // map number
              << ushort(20) // chunk width
              << ushort(20) // chunk height
              << int32(0) // origin chunk pos X
              << int32(0) // origin chunk pos Y
              << ushort(1) // number of chunks
              << byte(0); // adjacency bitmask

    // send map chunk
    vector<vector<int> > tiles(20,vector<int>(20,-1));
    gen_maze_map(tiles,make_pair(0,0));
    binstdout << netobjs["mapA.1"]; // netobj id
    for (int i = 0;i < 20;++i) {
        for (int j = 0;j < 20;++j) {
            binstdout << ushort(tiles[i][j]) // tile id
                      << byte(0); // pok_tile_warp_none
        }
    }
}
