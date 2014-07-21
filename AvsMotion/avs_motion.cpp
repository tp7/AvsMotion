#include <Windows.h>
#include <cstdio>
#include <string>
#include <vector>
#include <tuple>
#include <fstream>
#include <avisynth.h>
#include <boost/algorithm/string.hpp>

using namespace std;

const int PAD_WIDTH = 4;
const char* RESIZE_FILTER = "Spline36Resize";

typedef tuple<float, float, float> Aae3dData;

static string get_file_contents(const char *filename)
{
    ifstream in(filename, ios::in);
    if (!in)
    {
        return string();
    }
    string contents;
    in.seekg(0, ios::end);
    contents.resize(in.tellg());
    in.seekg(0, ios::beg);
    in.read(&contents[0], contents.size());
    in.close();
    return(contents);
}

static void parse_aae_block(vector<string>::iterator &it, vector<Aae3dData> &data_block)
{
    while (boost::algorithm::starts_with(*it, "\t"))
    {
        vector<string> values;
        boost::algorithm::trim_if(*it, boost::algorithm::is_any_of("\t "));
        boost::algorithm::split(values, *it, boost::algorithm::is_any_of("\t"));

        data_block.emplace_back(
            stof(values[1].c_str()),
            stof(values[2].c_str()),
            stof(values[3].c_str())
            );
        it++;
    }
}

struct MotionData
{
    vector<Aae3dData> position;
    vector<Aae3dData> scale;

    bool contains_frame(unsigned int frame) const
    {
        return position.size() > frame;
    }

    Aae3dData get_frame_offset(unsigned int frame) const;
};

Aae3dData MotionData::get_frame_offset(unsigned int frame) const
{
    auto frame_data = position[frame];
    auto first_frame_data = position[0];

    return make_tuple(
        get<0>(first_frame_data) -get<0>(frame_data),
        get<1>(first_frame_data) -get<1>(frame_data),
        get<2>(first_frame_data) -get<2>(frame_data)
        );
}

class AvsMotion : public GenericVideoFilter
{
public:
    AvsMotion(PClip mask, const char *motion_file, int frame_offset, bool mirror, int pad_color, IScriptEnvironment *env);
    PVideoFrame __stdcall GetFrame(int n, IScriptEnvironment* env);

private:
    PClip _padded;
    MotionData _motion_data;
    int _padded_width;
    int _padded_height;
    int _frame_offset;
    int _pad_color;
    bool _add_borders;
};

AvsMotion::AvsMotion(PClip mask, const char *motion_file, int frame_offset, bool mirror, int pad_color, IScriptEnvironment *env)
    : GenericVideoFilter(mask), _frame_offset(frame_offset), _add_borders(!mirror), _pad_color(pad_color)
{

    if (!env->FunctionExists(RESIZE_FILTER) ||
        !env->FunctionExists("AddBorders") ||
        !env->FunctionExists("Crop"))
    {
        env->ThrowError("Couldn't find one of the required functions: Spline36Resize, AddBorders, Crop");
    }

    if (motion_file == nullptr)
    {
        env->ThrowError("You should specity input file");
    }

    auto content = get_file_contents(motion_file);
    if (content.empty())
    {
        env->ThrowError("Nonexistent or empty input file");
    }
    if (!boost::algorithm::starts_with(content, "Adobe After Effects 6.0 Keyframe Data"))
    {
        env->ThrowError("Invalid input file");
    }

    vector<string> lines;
    boost::algorithm::split(lines, content, boost::algorithm::is_any_of("\n"));

    for (auto it = begin(lines); it != end(lines); ++it)
    {
        if (boost::algorithm::starts_with(*it, "Position"))
        {
            it += 2;
            parse_aae_block(it, _motion_data.position);
            break;
        }
    }

    _padded = child;
    if (_add_borders)
    {
        AVSValue add_border_args[6] = { child, PAD_WIDTH, PAD_WIDTH, PAD_WIDTH, PAD_WIDTH, _pad_color };
        _padded = env->Invoke("AddBorders", AVSValue(add_border_args, 6)).AsClip();
    }

    _padded_width = _padded->GetVideoInfo().width;
    _padded_height = _padded->GetVideoInfo().height;
}



PVideoFrame AvsMotion::GetFrame(int n, IScriptEnvironment* env)
{
    int frame = n - _frame_offset;
    if (frame < 0 || !_motion_data.contains_frame(frame))
    {
        return child->GetFrame(0, env);
    }

    auto offset = _motion_data.get_frame_offset(frame);

    auto workclip = _padded;

    AVSValue resize_args[7] = { workclip, _padded_width, _padded_height, get<0>(offset), get<1>(offset), _padded_width, _padded_height };
    workclip = env->Invoke(RESIZE_FILTER, AVSValue(resize_args, 7)).AsClip();

    if (_add_borders)
    {
        AVSValue crop_args[5] = { workclip, PAD_WIDTH, PAD_WIDTH, -PAD_WIDTH, -PAD_WIDTH };
        workclip = env->Invoke("Crop", AVSValue(crop_args, 5)).AsClip();
    }
    return workclip->GetFrame(n, env);
}


AVSValue __cdecl create_avs_motion(AVSValue args, void*, IScriptEnvironment* env)
{
    enum { MASK, MOTION_FILE, FRAME_OFFSET, MIRROR, PAD_COLOR };
    return new AvsMotion(args[MASK].AsClip(), args[MOTION_FILE].AsString(nullptr), args[FRAME_OFFSET].AsInt(0), args[MIRROR].AsBool(false), args[PAD_COLOR].AsInt(0), env);
}

const AVS_Linkage *AVS_linkage = nullptr;

extern "C" __declspec(dllexport) const char* __stdcall AvisynthPluginInit3(IScriptEnvironment* env, const AVS_Linkage* const vectors)
{
    AVS_linkage = vectors;

    env->AddFunction("AvsMotion", "c[file]s[offset]i[mirror]b[pad_color]i", create_avs_motion, 0);
    return "All bugs are courtesy of torque";
}