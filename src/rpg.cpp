static Arena scratch_arena;
static Arena persist_arena;

void *ArenaAlloc(Arena *arena, u64 count);
Arena CreateArena(void *base_address, u64 size);

struct Camera {
    v3 position;
    v3 target;
};

struct GameState {
    b32 initialized;

    Mesh cube;
    Camera camera;
    mat4 projection;
    Texture wall;

    v3 hero_position;
};

void DrawMesh(Mesh, v3, v4);
Camera *GetCamera();

void UpdateAndRender() {
    GameState *state = (GameState *)platform.memory;

    if (!state->initialized) {
        u64 free_bytes = platform.memory_size - sizeof(GameState);
        // initialize game state
        scratch_arena = CreateArena((u8*)platform.memory + sizeof(GameState), free_bytes/2);
        persist_arena = CreateArena((u8*)platform.memory + sizeof(GameState) + free_bytes/2, free_bytes/2);

        Mesh cube = {};
        glGenVertexArrays(1, &cube.vao);
        glGenBuffers(1,  &cube.vbo);
        glBindVertexArray(cube.vao);
        glBindBuffer(GL_ARRAY_BUFFER, cube.vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(cube_vertices), cube_vertices, GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(v3) + sizeof(v2), 0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(v3) + sizeof(v2), (void *)(sizeof(v3)));
        glEnableVertexAttribArray(1);
        cube.vertex_count = sizeof(cube_vertices) / (sizeof(v3) + sizeof(v2));
        state->cube = cube;

        Camera *camera = &state->camera;
        camera->position = {0,0,3};
        camera->target = {};

        int w,h;
        glfwGetFramebufferSize(platform.window, &w, &h);

        state->projection = perspective(radians(45.0f), (float)w / (float)h, 0.1f, 100.0f);

        LoadTexture("assets/wall.jpg", &state->wall);

        glClearColor(0, 0,0,0);

        state->initialized = true;
    }

    /* TODO:
        [x] draw a textured 3D cube
        [x] move textured cube around
        [] orbit camera controls attached to cube
        [] fill scene with objects
        [] lighting
        [] entity parent-child relationships
        [] particle system entity
        [] culling (collision volumes)
        [] create 3D model
        [] rig 3D model
        [] render and animate 3D model in-engine
        [] make a game
    */

#if 0
    Camera *camera = GetCamera();
    float radius = 10.0f;
    float camX = static_cast<float>(sin(glfwGetTime()) * radius);
    float camZ = static_cast<float>(cos(glfwGetTime()) * radius);
    camera->position = glm::vec3(camX, 0.0f, camZ);
    camera->target = {};
#endif

    if (IsKeyPressed(GLFW_KEY_W)) {
        state->hero_position.z -= 1 * platform.delta_time;
    }

    if (IsKeyPressed(GLFW_KEY_S)) {
        state->hero_position.z += 1 * platform.delta_time;
    }

    if (IsKeyPressed(GLFW_KEY_Q)) {
        state->hero_position.x -= 1 * platform.delta_time;
    }

    if (IsKeyPressed(GLFW_KEY_E)) {
        state->hero_position.x += 1 * platform.delta_time;
    }

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    u32 temp = glutil_sampler_2d;
    glutil_sampler_2d = state->wall.id;
    DrawMesh(state->cube, state->hero_position, {1,1,1,1});
    glutil_sampler_2d = temp;
}


void *ArenaAlloc(Arena *arena, u64 count) {
    if (arena->count + count > arena->size) {
        u64 new_size = arena->size * 2;
        arena->base_address = realloc(arena->base_address, new_size);
        fprintf(stderr, "INFO: Arena resized: (%lld -> %lld)\n", arena->size, new_size);
        arena->size = new_size;
    }

    void *result = (u8 *)arena->base_address + arena->count;
    arena->count += count;

    memset(result, 0, count);

    return result;
}

Arena CreateArena(u64 size) {
    Arena result = {};
    result.base_address = malloc(size);
    result.size = size;

    return result;
}

Arena CreateArena(void *base_address, u64 size) {
    Arena result = {};
    result.base_address = base_address;
    result.size = size;

    return result;
}

void GetProjectionTransform(mat4 *m) {
    GameState *state = (GameState *)platform.memory;
    *m = state->projection;
}

void GetCameraDirection(Camera *camera, v3 *direction) {
    v3 result = normalize(camera->position - camera->target);
    *direction = result;
}

void GetCameraTransform(Camera *camera, mat4 *t) {
    mat4 result = lookAt(camera->position, camera->target, {0,1,0});
    *t = result;
}

Camera *GetCamera() {
    GameState *state = (GameState *)platform.memory;
    return &state->camera;
}

void DrawMesh(Mesh mesh, v3 position, v4 color) {
    static b32 initialized = false;
    static u32 program = 0;

    if (!initialized) {
        const char *vs_source = R"(
            #version 460
            layout (location = 0) in vec3 p;
            layout (location = 1) in vec2 uv;

            out vec2 vuv;

            uniform mat4 view;
            uniform mat4 model;
            uniform mat4 projection;

            void main() {
                gl_Position = projection * view * model * vec4(p, 1.0);
                vuv = uv;
            }
        )";

        const char *fs_source = R"(
            #version 460
            in vec2 vuv;
            out vec4 frag_color;

            uniform vec4 color;
            uniform sampler2D diffuse;

            void main() {
                frag_color = texture(diffuse, vuv) * color;
            }
        )";

        GLuint vs = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vs, 1, &vs_source, 0);

        GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fs, 1, &fs_source, 0);

        CompileShader(vs);
        CompileShader(fs);

        program = glCreateProgram();
        glAttachShader(program, vs);
        glAttachShader(program, fs);
        LinkProgram(program);
        glDeleteShader(vs);
        glDeleteShader(fs);

        initialized = true;
    }

    Camera *camera = GetCamera();

    mat4 view;
    GetCameraTransform(camera, &view);

    mat4 projection;
    GetProjectionTransform(&projection);

    mat4 model = mat4(1.0f);
    model = translate(model, position);

    glUseProgram(program);
    u32 color_location = glGetUniformLocation(program, "color");
    glUniform4fv(color_location, 1, (f32*)&color);

    u32 view_location = glGetUniformLocation(program, "view");
    glUniformMatrix4fv(view_location, 1, GL_FALSE, (f32*)&view);

    u32 projection_location = glGetUniformLocation(program, "projection");
    glUniformMatrix4fv(projection_location, 1, GL_FALSE, (f32*)&projection);

    u32 model_location = glGetUniformLocation(program, "model");
    glUniformMatrix4fv(model_location, 1, GL_FALSE, (f32*)&model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, glutil_sampler_2d);

    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertex_count);
}