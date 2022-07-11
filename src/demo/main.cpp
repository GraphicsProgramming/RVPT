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

        const auto dt = engine.since_last_frame();

        auto translation = glm::vec3();

        if (window.is_key_held(demo::window::KeyCode::KEY_W))
            translation.z += 1.0f;
        if (window.is_key_held(demo::window::KeyCode::KEY_S))
            translation.z -= 1.0f;

        if (window.is_key_held(demo::window::KeyCode::KEY_D))
            translation.x += 1.0f;
        if (window.is_key_held(demo::window::KeyCode::KEY_A))
            translation.x -= 1.0f;

        translation *= static_cast<float>(dt);

        auto rotation = glm::vec3();
        if (window.is_key_down(demo::window::KeyCode::KEY_RIGHT)) rotation.x = 0.3f;
        if (window.is_key_down(demo::window::KeyCode::KEY_LEFT)) rotation.x = -0.3f;
        if (window.is_key_down(demo::window::KeyCode::KEY_UP)) rotation.y = 0.3f;
        if (window.is_key_down(demo::window::KeyCode::KEY_DOWN)) rotation.y = -0.3f;

        rotation *= static_cast<float>(dt);

        engine.update(trianges, translation, rotation);
        engine.draw();
    }
}