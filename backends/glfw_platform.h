// GLFW platform glue: forwards window input to a tg::Context and runs an event-driven loop.
#pragma once

struct GLFWwindow;

namespace tg
{
    class Context;

    namespace glfw
    {
        // Hook GLFW callbacks up to Ctx (uses the window user pointer).
        void Install( GLFWwindow* Window, Context& Ctx );

        // Keep viewport/DPI/cursor in sync. Call once per loop iteration before Frame().
        void Sync( GLFWwindow* Window, Context& Ctx );

        // Block until there is something to do (input, a posted task, an animation, a timed redraw).
        void WaitForWork( Context& Ctx );
    }
}
