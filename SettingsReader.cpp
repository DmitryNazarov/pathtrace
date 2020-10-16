#include <iostream>
#include <unordered_set>
#include <SettingsReader.h>


uint32_t add_to_vertices(std::vector<vec3>& vertices, vec3 vertex1)
{
  auto it = std::find(vertices.begin(), vertices.end(), vertex1);
  if (it != vertices.end())
  {
    return std::distance(vertices.begin(), it);
  }
  vertices.push_back(vertex1);
  return vertices.size() - 1;
}

Settings read_settings(const std::string& filename) {
  Settings settings;

  std::vector<vec3> scene_vertices;
  std::vector<std::pair<vec3, vec3>> vertex_normals;

  Color ambient{ 0.2f, 0.2f, 0.2f, 1.0f };
  Color diffuse{ 0.0f, 0.0f, 0.0f, 0.0f };
  Color specular{ 0.0f, 0.0f, 0.0f, 0.0f };
  Color emission{ 0.0f, 0.0f, 0.0f, 0.0f };
  float shininess = .0;
  vec3 attenuation{ 1.0f, 0.0f, 0.0f };

  std::string str, cmd;
  std::ifstream in(filename);
  if (!in.is_open())
    return settings;

  std::stack<mat4> transfstack;
  transfstack.push(mat4(1.0)); // identity

  while (getline(in, str)) {
    if ((str.find_first_not_of(" \t\r\n") == std::string::npos) ||
      (str[0] == '#'))
      continue;

    std::stringstream ss(str);
    ss >> cmd;

    if (cmd == "size") {
      int values[2];
      if (readvals(ss, 2, values)) {
        settings.width = values[0];
        settings.height = values[1];
        settings.aspect = static_cast<float>(settings.width) /
          static_cast<float>(settings.height);
      }
    }
    else if (cmd == "camera") {
      float values[10];
      if (readvals(ss, 10, values)) {
        settings.eye_init = vec3(values[0], values[1], values[2]);
        settings.center = vec3(values[3], values[4], values[5]);
        settings.up_init = vec3(values[6], values[7], values[8]);
        settings.fovy = values[9];

        settings.w = normalize(settings.eye_init - settings.center);
        settings.u = normalize(cross(settings.up_init, settings.w));
        settings.v = cross(settings.w, settings.u);
      }
    }
    else if (cmd == "maxdepth") {
      int value;
      if (readvals(ss, 1, &value)) {
        settings.depth = value;
      }
    }
    else if (cmd == "output") {
      std::string value;
      if (readvals(ss, 1, &value)) {
        settings.filename = value;
      }
    }
    else if (cmd == "sphere") {
      continue; //fix it

      //float values[4];
      //if (readvals(ss, 4, values)) {
      //  Sphere s;
      //  s.radius = values[3];
      //  s.pos = vec3(values[0], values[1], values[2]);
      //  s.transform = transfstack.top();
      //  s.inverted_transform = inverse(transfstack.top());
      //  s.material.ambient = ambient;
      //  s.material.diffuse = diffuse;
      //  s.material.specular = specular;
      //  s.material.emission = emission;
      //  s.material.shininess = shininess;
      //  settings.spheres.push_back(s);
      //  Object o;
      //  o.type = SPHERE;
      //  o.index = settings.spheres.size() - 1;
      //  settings.objects.push_back(o);
      //}
    }
    else if (cmd == "translate") {
      float values[3];
      if (readvals(ss, 3, values)) {
        transfstack.top() = translate(transfstack.top(), vec3(values[0], values[1], values[2]));
      }
    }
    else if (cmd == "scale") {
      float values[3];
      if (readvals(ss, 3, values)) {
        transfstack.top() = scale(transfstack.top(), vec3(values[0], values[1], values[2]));
      }
    }
    else if (cmd == "rotate") {
      float values[4];
      if (readvals(ss, 4, values)) {
        vec3 axis = normalize(vec3(values[0], values[1], values[2]));
        transfstack.top() = rotate(transfstack.top(), radians(values[3]), axis);
      }
    }
    else if (cmd == "pushTransform") {
      transfstack.push(transfstack.top());
    }
    else if (cmd == "popTransform") {
      if (transfstack.size() <= 1) {
        std::cerr << "Stack has no elements.  Cannot Pop\n";
      }
      else {
        transfstack.pop();
      }
    }
    else if (cmd == "vertex") {
      float values[3];
      if (readvals(ss, 3, values)) {
        scene_vertices.emplace_back(values[0], values[1], values[2]);

        //auto res = scene_vertices.insert({ values[0], values[1], values[2] });
        //if (res.second == false)
        //{
        //  std::cerr << "Can't insert vertex, possible duplicate\n";
        //}
      }
    }
    else if (cmd == "vertexnormal") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto vertex = vec3(values[0], values[1], values[2]);
        auto vertex_normal = vec3(values[3], values[4], values[5]);
        vertex_normals.emplace_back(vertex, vertex_normal);
      }
    }
    else if (cmd == "tri") {
      int values[3];
      if (readvals(ss, 3, values)) {
        uint32_t index0 = add_to_vertices(settings.vertices, transfstack.top() * vec4(scene_vertices[values[0]], 1.0f));
        settings.indices.push_back(index0);
        uint32_t index1 = add_to_vertices(settings.vertices, transfstack.top() * vec4(scene_vertices[values[1]], 1.0f));
        settings.indices.push_back(index1);
        uint32_t index2 = add_to_vertices(settings.vertices, transfstack.top() * vec4(scene_vertices[values[2]], 1.0f));
        settings.indices.push_back(index2);
        //normal = normalize(cross(t.vertices[1] - t.vertices[0], t.vertices[2] - t.vertices[0]));

        Object o;
        Material m;
        m.ambient = ambient;
        m.diffuse = diffuse;
        m.specular = specular;
        m.emission = emission;
        m.shininess = shininess;
        o.type = TRIANGLE;
        o.indices = std::vector<size_t>{index0, index1, index2};
        o.material = m;
        settings.objects.push_back(o);
      }
    }
    //else if (cmd == "trinormal") {
    //  int values[3];
    //  if (readvals(ss, 3, values)) {
    //    TriangleNormals t;
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[0]].first, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[1]].first, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[2]].first, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[0]].second, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[1]].second, 1.0f));
    //    t.vertices.push_back(transfstack.top() *
    //      vec4(vertex_normals[values[2]].second, 1.0f));
    //    t.material.ambient = ambient;
    //    t.material.diffuse = diffuse;
    //    t.material.specular = specular;
    //    t.material.emission = emission;
    //    t.material.shininess = shininess;
    //    settings.triangle_normals.push_back(t);
    //    Object o;
    //    o.type = TRIANGLE_NORMALS;
    //    o.index = settings.triangle_normals.size() - 1;
    //    settings.objects.push_back(o);
    //  }
    //}
    else if (cmd == "directional") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto dir = vec3(values[0], values[1], values[2]);
        auto c = Color(values[3], values[4], values[5], 1.0f);
        settings.direct_lights.emplace_back(dir, c);
      }
    }
    else if (cmd == "point") {
      float values[6];
      if (readvals(ss, 6, values)) {
        auto pos = vec3(values[0], values[1], values[2]);
        auto c = Color(values[3], values[4], values[5], 1.0f);
        settings.point_lights.emplace_back(pos, c, attenuation);
      }
    }
    else if (cmd == "ambient") {
      float values[3];
      if (readvals(ss, 3, values)) {
        ambient = Color(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "attenuation") {
      float values[3];
      if (readvals(ss, 3, values)) {
        attenuation[0] = values[0];
        attenuation[1] = values[1];
        attenuation[2] = values[2];
      }
    }
    else if (cmd == "diffuse") {
      float values[3];
      if (readvals(ss, 3, values)) {
        diffuse = Color(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "specular") {
      float values[3];
      if (readvals(ss, 3, values)) {
        specular = Color(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "emission") {
      float values[3];
      if (readvals(ss, 3, values)) {
        emission = Color(values[0], values[1], values[2], 1.0f);
      }
    }
    else if (cmd == "shininess") {
      float value;
      if (readvals(ss, 1, &value)) {
        shininess = value;
      }
    }
    else if (cmd == "maxverts" || cmd == "maxvertnorms") {
      continue;
    }
    else {
      std::cerr << "Unknown Command: " << cmd << " Skipping \n";
    }
  }

  return settings;
}