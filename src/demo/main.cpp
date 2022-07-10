#include "window.h"
#include "engine.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tinyobjloader/tiny_obj_loader.h>

std::vector<glm::vec3> load_model(std::string inputfile)
{
    auto triangles = std::vector<glm::vec3>();

    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;

    std::string warn;
    std::string err;

    tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, inputfile.c_str());

    if (!warn.empty())
    {
        fmt::print("[{}: {}] {}\n", "WARNING", "MODEL-LOADING", warn);
    }

    if (!err.empty())
    {
        fmt::print("[{}: {}] {}\n", "ERROR", "MODEL-LOADING", err);
        exit(-1);
    }

    // Loop over shapes
    for (auto & shape : shapes) {
        // Loop over faces(polygon)
        size_t index_offset = 0;
        for (size_t f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
            const auto fv = shape.mesh.num_face_vertices[f];

            // Loop over vertices in the face.
            if (fv != 3)
            {
                fmt::print("Shape had a face with more than 3 vertices, skipping");
                continue;
            }
            glm::vec3 vertices[3];

            for (size_t v = 0; v < fv; v++) {
                tinyobj::index_t idx = shape.mesh.indices[index_offset + v];
                vertices[v].x = attrib.vertices[3*idx.vertex_index+0];
                vertices[v].y = attrib.vertices[3*idx.vertex_index+1];
                vertices[v].z = attrib.vertices[3*idx.vertex_index+2];
            }
            index_offset += fv;

            triangles.push_back(vertices[0]);
            triangles.push_back(vertices[1]);
            triangles.push_back(vertices[2]);
        }
    }

    return triangles;
}

int main()
{
    auto window = demo::window("RVPT Demo", 1920, 1080);

    auto engine = demo::engine(&window);
    engine.init();

    const auto trianges = load_model("./assets/models/rabbit.obj");

    while (!window.should_close())
    {
        window.poll();

        engine.update(trianges);
        engine.draw();
    }
}