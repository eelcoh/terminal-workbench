/* Upstream helpers operate in a pane-local canvas. Outside a bound pane these
 * wrappers use the real window. This adapter does not alter Raylib globally. */
static Rectangle client_surface;
static bool client_surface_active;
static int surface_width(void)
{
    return client_surface_active ? (int)client_surface.width : GetScreenWidth();
}
static int surface_height(void)
{
    return client_surface_active ? (int)client_surface.height : GetScreenHeight();
}
static Vector2 surface_pointer(void)
{
    Vector2 pointer = GetMousePosition();
    if (client_surface_active) {
        pointer.x -= client_surface.x;
        pointer.y -= client_surface.y;
    }
    return pointer;
}
#define GetScreenWidth surface_width
#define GetScreenHeight surface_height
#define GetMousePosition surface_pointer
