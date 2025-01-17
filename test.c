#include <ApplicationServices/ApplicationServices.h>

int main(void) {
    // Create a transparent window covering the entire screen
    CGRect screenBounds = CGDisplayBounds(CGMainDisplayID());
    CGWindowID windowID = CGWindowCreate(kCGNullWindowID, kCGWindowSurfaceShadowless);
    CGWindowSetAlpha(windowID, 0.5); // Set window transparency
    
    // Get a graphics context for drawing
    CGContextRef context = CGWindowContextCreate(windowID, 0);
    
    // Draw something (e.g., a red rectangle)
    CGContextSetRGBFillColor(context, 1.0, 0.0, 0.0, 1.0);
    CGContextFillRect(context, CGRectMake(100, 100, 200, 200));
    
    // Flush drawing to the screen
    CGContextFlush(context);
    
    // Keep the program running
    while(1) {
        sleep(1);
    }
    
    return 0;
}
