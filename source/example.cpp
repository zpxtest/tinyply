// This software is in the public domain. Where that dedication is not
// recognized, you are granted a perpetual, irrevocable license to copy,
// distribute, and modify this file as you see fit.
// https://github.com/ddiakopoulos/tinyply
// Version 2.4

// The purpose of this file is to demonstrate the tinyply API and provide several almost-complete
// functions that can be copied and pasted into your own application or library. Because tinyply
// treats the file format as structured data, it's up to you to copy or move the parsed data
// into your application-specific data structures (e.g. float3, vec3, etc). 

#include "tinyply.h"
using namespace tinyply;

#include "example-utils.hpp"

void write_ply_example(const std::string & filename)
{
    geometry cube = make_cube_geometry();

    std::filebuf fb_binary;
    fb_binary.open(filename + "-binary.ply", std::ios::out | std::ios::binary);
    std::ostream outstream_binary(&fb_binary);
    if (outstream_binary.fail()) throw std::runtime_error("failed to open " + filename);

    std::filebuf fb_ascii;
    fb_ascii.open(filename + "-ascii.ply", std::ios::out);
    std::ostream outstream_ascii(&fb_ascii);
    if (outstream_ascii.fail()) throw std::runtime_error("failed to open " + filename);

    PlyFile cube_file;

    cube_file.add_properties_to_element("vertex", { "x", "y", "z" }, 
        Type::FLOAT32, cube.vertices.size(), reinterpret_cast<uint8_t*>(cube.vertices.data()), Type::INVALID, 0);

    cube_file.add_properties_to_element("vertex", { "nx", "ny", "nz" },
        Type::FLOAT32, cube.normals.size(), reinterpret_cast<uint8_t*>(cube.normals.data()), Type::INVALID, 0);

    cube_file.add_properties_to_element("vertex", { "u", "v" },
        Type::FLOAT32, cube.texcoords.size() , reinterpret_cast<uint8_t*>(cube.texcoords.data()), Type::INVALID, 0);

    cube_file.add_properties_to_element("face", { "vertex_indices" },
        Type::UINT32, cube.triangles.size(), reinterpret_cast<uint8_t*>(cube.triangles.data()), Type::UINT8, 3);

    cube_file.get_comments().push_back("generated by tinyply 2.4");

    // Write an ASCII file
    cube_file.write(outstream_ascii, false);

    // Write a binary file
    cube_file.write(outstream_binary, true);
}

void read_ply_file(const std::string & filepath, uint32_t list_hint = 0, bool preload_into_memory = true, bool print_header = true)
{
    manual_timer read_timer;
    std::cout << "........................................................................\n";
    std::cout << "Now Reading: " << filepath << std::endl;

    std::unique_ptr<std::istream> file_stream;
    std::vector<uint8_t> byte_buffer;

    float preload_into_memory_time = 0.f;

    try
    {
        // For most files, pre-loading the entire file into memory upfront and wrapping it into a 
        // stream is a net win for import speed, ~30-40% faster. This feature requires some utility
        // classes in the `example-utils.hpp` header. 
        if (preload_into_memory)
        {
            read_timer.start();
            byte_buffer = read_file_binary(filepath);
            file_stream.reset(new memory_stream((char*)byte_buffer.data(), byte_buffer.size()));
            read_timer.stop();
            preload_into_memory_time += ((float) read_timer.get() / 1000.f);
        }
        else
        {
            file_stream.reset(new std::ifstream(filepath, std::ios::binary));
        }

        if (!file_stream || file_stream->fail()) throw std::runtime_error("file_stream failed to open " + filepath);

        file_stream->seekg(0, std::ios::end);
        const float size_mb = file_stream->tellg() * float(1e-6);
        file_stream->seekg(0, std::ios::beg);

        PlyFile file;
        file.parse_header(*file_stream);

        if (print_header)
        {
            std::cout << "\t[ply_header] Type: " << (file.is_binary_file() ? "binary" : "ascii") << std::endl;
            for (const auto & c : file.get_comments()) std::cout << "\t[ply_header] Comment: " << c << std::endl;
            for (const auto & c : file.get_info()) std::cout << "\t[ply_header] Info: " << c << std::endl;

            for (const auto & e : file.get_elements())
            {
                std::cout << "\t[ply_header] element: " << e.name << " (" << e.size << ")" << std::endl;
                for (const auto & p : e.properties)
                {
                    std::cout << "\t[ply_header] \tproperty: " << p.name << " (type=" << tinyply::PropertyTable[p.propertyType].str << ")";
                    if (p.isList) std::cout << " (list_type=" << tinyply::PropertyTable[p.listType].str << ")";
                    std::cout << std::endl;
                }
            }
        }

        // Because most people have their own mesh types, tinyply treats parsed data as structured/typed 
        // byte buffers. See examples below on how to marry your own application data structures with this one. 
        // Note that buffers will be populated with properties in the order they appear in the
        // header, and no swizzling will be performed (e.g. if {"z", "y", "x"} is requested, the the items 
        // in PlyData will be {"x", "y", "z"} if that's how they are ordered in the header). 
        std::shared_ptr<PlyData> vertices, normals, colors, texcoords, faces, tripstrip;

        // The header information can be used to programmatically extract properties on elements
        // known to exist in the header prior to reading the data. However, the vast majority
        // of ply exporters only use a handful of common properties such as the following.  
        try { vertices = file.request_properties_from_element("vertex", { "x", "y", "z" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { normals = file.request_properties_from_element("vertex", { "nx", "ny", "nz" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { colors = file.request_properties_from_element("vertex", { "red", "green", "blue", "alpha" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { colors = file.request_properties_from_element("vertex", { "r", "g", "b", "a" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        try { texcoords = file.request_properties_from_element("vertex", { "u", "v" }); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        // Providing a list size hint (the last argument) is a 2x performance improvement. If you have 
        // arbitrary ply files, it is best to leave this 0. 
        try { faces = file.request_properties_from_element("face", { "vertex_indices" }, list_hint); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        // Tristrips must always be read with a 0 list size hint (unless you know exactly how many elements
        // are specifically in the file, which is unlikely); 
        try { tripstrip = file.request_properties_from_element("tristrips", { "vertex_indices" }, 0); }
        catch (const std::exception & e) { std::cerr << "tinyply exception: " << e.what() << std::endl; }

        // Example of the reading progress callback API
        if (true)
        {
            auto progress_callback = [](const tinyply::ProgressCallbackInfo info)
            {
                std::cout << "\tcallback %: " << ((float)info.current_bytes / (float)info.total_bytes) * 100.f << "\n";
            };
            file.set_progress_callback((size_t)1e+7, progress_callback); // every 10mb
        }

        read_timer.start();
        try { file.read(*file_stream); } // might throw in very rare (typically malformed file) cases
        catch (const std::exception& e) { std::cerr << "tinyply read exception: " << e.what() << std::endl; }
        read_timer.stop();

        const float parsing_time = preload_into_memory_time + ((float)read_timer.get() / 1000.f);
        std::cout << "\tparsing " << size_mb << "mb in " << parsing_time << " seconds [" << (size_mb / parsing_time) << " MBps]" << std::endl;

        if (vertices)   std::cout << "\tRead " << vertices->count  << " total vertices "<< std::endl;
        if (normals)    std::cout << "\tRead " << normals->count   << " total vertex normals " << std::endl;
        if (colors)     std::cout << "\tRead " << colors->count << " total vertex colors " << std::endl;
        if (texcoords)  std::cout << "\tRead " << texcoords->count << " total vertex texcoords " << std::endl;
        if (faces)      std::cout << "\tRead " << faces->count     << " total faces (triangles) " << std::endl;
        if (tripstrip)  std::cout << "\tRead " << (tripstrip->buffer.size_bytes() / tinyply::PropertyTable[tripstrip->t].stride) << " total indicies (tristrip) " << std::endl;

        // Example One: converting to your own application types
        {
            const size_t numVerticesBytes = vertices->buffer.size_bytes();
            std::vector<float3> verts(vertices->count);
            std::memcpy(verts.data(), vertices->buffer.get(), numVerticesBytes);
        }

        // Example Two: converting to your own application type
        {
            std::vector<float3> verts_floats;
            std::vector<double3> verts_doubles;
            if (vertices->t == tinyply::Type::FLOAT32) { /* as floats ... */ }
            if (vertices->t == tinyply::Type::FLOAT64) { /* as doubles ... */ }
        }

        // Example: variable length lists
        {
            struct u3 { uint32_t x, y, z; };
            struct u4 { uint32_t x, y, z, w; };

            std::vector<u3> triangles;
            std::vector<u4> quads;
            std::vector<std::vector<uint32_t>> n_gons;

            if (faces->list_indices.size() > 0)
            {
                size_t offset = 0;
                for (const auto & index_length : faces->list_indices)
                {
                    if (index_length == 3)
                    {
                        u3 tri;
                        std::memcpy(&tri, faces->buffer.get() + offset, sizeof(u3));
                        triangles.push_back(tri);
                        offset += sizeof(u3);
                    }
                    else if (index_length == 4)
                    {
                        u4 quad;
                        std::memcpy(&quad, faces->buffer.get() + offset, sizeof(u4));
                        quads.push_back(quad);
                        offset += sizeof(u4);
                    }
                    else
                    {
                        std::vector<uint32_t> ngon(index_length);
                        auto bytes_in_ngon = tinyply::PropertyTable[faces->t].stride * index_length;
                        std::memcpy(ngon.data(), faces->buffer.get() + offset, bytes_in_ngon);
                        offset += bytes_in_ngon;
                        n_gons.push_back(ngon);
                    }
                }

                if (triangles.size())   std::cout << "\tRead " << triangles.size() << " total triangles " << std::endl;
                if (quads.size())       std::cout << "\tRead " << quads.size() << " total quads " << std::endl;
                if (n_gons.size())      std::cout << "\tRead " << n_gons.size() << " total n_gons " << std::endl;
            }
        }
    }
    catch (const std::exception & e)
    {
        std::cerr << "Caught tinyply exception: " << e.what() << std::endl;
    }
}

int main(int argc, char *argv[])
{
    (void) argc; (void) argv;

    // Circular write-read
    write_ply_example("example_cube");
    read_ply_file("example_cube-ascii.ply", 0);
    read_ply_file("example_cube-binary.ply", 0, true);

    //read_ply_file("../assets/lucy.ply", 3, true);
    //read_ply_file("../assets/validate/valid/kcrane.city.ply");

    return EXIT_SUCCESS;
}
