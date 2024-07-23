#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <counter_icons.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include "font.h"


#define FONT u8g2_font_inb33_mn // see font.h
#define MAX_COUNT 9999   // max counter value
#define VIBRO_TIME_MS 20 // vibration time 
#define OFFSET_X 32      // counter position by y
#define OFFSET_Y 64      // counter position by x

typedef struct {
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
    FuriMutex* mutex;
    NotificationApp* notifications;

    int count;
    bool pressed;
    bool vibro;
} Counter;

// Cleanup before exit
void state_free(Counter* c) {
    gui_remove_view_port(c->gui, c->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(c->view_port);
    furi_message_queue_free(c->input_queue);
    furi_mutex_free(c->mutex);
    free(c);
}

// User input callback
static void input_callback(InputEvent* input_event, void* ctx) {
    Counter* c = ctx;
    if(input_event->type == InputTypeShort || input_event->type == InputTypeLong) {
        furi_message_queue_put(c->input_queue, input_event, 0);
    }
}

// Update canvas callback
static void render_callback(Canvas* canvas, void* ctx) {
    Counter* c = ctx;
    furi_check(furi_mutex_acquire(c->mutex, FuriWaitForever) == FuriStatusOk);
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_custom_u8g2_font(canvas, FONT);

    char scount[8];
    snprintf(scount, sizeof(scount), "%d", c->count);
    canvas_draw_str_aligned(canvas, OFFSET_Y, OFFSET_X , AlignCenter, AlignCenter, scount);
    furi_mutex_release(c->mutex);
}

// App Initialise
Counter* init() {
    Counter* c = malloc(sizeof(Counter));

    c->count = 0;     // counter start value
    c->vibro = false; // vibro is disabled by default
    c->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    c->view_port = view_port_alloc();
    c->gui = furi_record_open(RECORD_GUI);
    c->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    c->notifications = furi_record_open(RECORD_NOTIFICATION);
    view_port_input_callback_set(c->view_port, input_callback, c);
    view_port_draw_callback_set(c->view_port, render_callback, c);
    gui_add_view_port(c->gui, c->view_port, GuiLayerFullscreen);
    return c;
}

// Send vibro notification
static void vibro(Counter* c) {
    if (c->vibro) {
        notification_message(c->notifications, &sequence_set_vibro_on);
        furi_delay_ms(VIBRO_TIME_MS);
        notification_message(c->notifications, &sequence_reset_vibro);
    }
}

// App
int32_t counter(void) {
    Counter* c = init();

    InputEvent input;
    for(bool processing = true; processing;) {
        while(furi_message_queue_get(c->input_queue, &input, FuriWaitForever) == FuriStatusOk) {
            furi_check(furi_mutex_acquire(c->mutex, FuriWaitForever) == FuriStatusOk);

            if(input.type == InputTypeShort) {
                switch(input.key) {
                   case InputKeyUp:
                    case InputKeyOk:
                        if (c->count < MAX_COUNT) {
                            c->pressed = true;
                            c->count++;
                            vibro(c);
                        }else{
                            notification_message(c->notifications, &sequence_blink_red_100);
                            furi_delay_ms(200);
                            vibro(c);
                        }
                        break;
                    case InputKeyDown:
                        if (c->count > 0) {
                            c->pressed = true;
                            c->count--;
                            vibro(c);
                        }else{
                            notification_message(c->notifications, &sequence_blink_red_100);
                            furi_delay_ms(200);
                            vibro(c);
                        }
                        break;
                    case InputKeyBack:
                        processing = false;
                        break;
                    case InputKeyLeft:
                    case InputKeyRight:
                        break;
                    default:
                        break;
                }
            } else if(input.type == InputTypeLong) {
                switch(input.key) {
                    case InputKeyBack:
                        c->count = 0;
                        break;
                    case InputKeyOk:
                        c->vibro = !c->vibro;
                        notification_message(c->notifications, &sequence_set_vibro_on);
                        furi_delay_ms(VIBRO_TIME_MS);
                        notification_message(c->notifications, &sequence_reset_vibro);
                        break;
                    case InputKeyLeft:
                    case InputKeyRight:
                    case InputKeyUp:
                    case InputKeyDown:
                        break;
                    default:
                        break;
                }
            }
            furi_mutex_release(c->mutex);
            if(!processing) {
                break;
            }
            view_port_update(c->view_port);
        }
    }
    state_free(c);
    return 0;
}
