#include <my_main.h>
#include <stddef.h>
#define BATTERY_CARD_X 256
#define BATTERY_CARD_SHOW_Y -13
#define BATTERY_CARD_HIDE_Y -43
#define BATTERY_CARD_WIDTH 40
#define BATTERY_CARD_WIDTH_CHARGING (40 + 16)
#define BATTERY_CARD_HEIGHT 33
extern "C" const lv_img_dsc_t bolt;

static MyCard card_Battery;
static lv_obj_t *img_bolt = NULL;
static lv_obj_t *battery_body = NULL;
static lv_obj_t *battery_fill = NULL;
static lv_obj_t *battery_label = NULL;
static lv_obj_t *battery_cap = NULL;

static int battery_percent_from_voltage(int16_t voltage)
{
    static const struct
    {
        int voltage;
        int percent;
    } curve[] = {
        {3500, 0}, {3600, 5}, {3700, 15}, {3800, 30},
        {3900, 50}, {4000, 70}, {4100, 85}, {4200, 100},
    };

    if (voltage <= curve[0].voltage)
        return curve[0].percent;
    for (size_t i = 1; i < sizeof(curve) / sizeof(curve[0]); ++i)
    {
        if (voltage <= curve[i].voltage)
        {
            int dv = curve[i].voltage - curve[i - 1].voltage;
            int dp = curve[i].percent - curve[i - 1].percent;
            return curve[i - 1].percent +
                   (voltage - curve[i - 1].voltage) * dp / dv;
        }
    }
    return 100;
}

static bool expanded = false;
static void battery_card_construct(lv_obj_t *parent)
{
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_opa(parent, LV_OPA_TRANSP, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_width(parent, 0, 0);
    battery_body = lv_obj_create(parent);
    lv_obj_clear_flag(battery_body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(battery_body, 30, 18);
    lv_obj_set_pos(battery_body, 4, 7);
    lv_obj_set_style_bg_opa(battery_body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(battery_body, 2, 0);
    lv_obj_set_style_border_color(battery_body, lv_color_white(), 0);
    lv_obj_set_style_radius(battery_body, 2, 0);

    battery_fill = lv_obj_create(battery_body);
    lv_obj_clear_flag(battery_fill, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(battery_fill, 26, 14);
    lv_obj_set_pos(battery_fill, 2, 2);
    lv_obj_set_style_bg_color(battery_fill, lv_color_white(), 0);
    lv_obj_set_style_border_width(battery_fill, 0, 0);
    lv_obj_set_style_radius(battery_fill, 1, 0);

    battery_label = lv_label_create(battery_body);
    lv_obj_set_size(battery_label, 26, 18);
    lv_obj_center(battery_label);
    lv_obj_set_y(battery_label, 1);
    lv_obj_set_style_text_align(battery_label, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_text(battery_label, "--");

    battery_cap = lv_obj_create(parent);
    lv_obj_clear_flag(battery_cap, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(battery_cap, 3, 8);
    lv_obj_set_pos(battery_cap, 34, 12);
    lv_obj_set_style_bg_color(battery_cap, lv_color_white(), 0);
    lv_obj_set_style_border_width(battery_cap, 0, 0);
    lv_obj_set_style_radius(battery_cap, 1, 0);

    img_bolt = lv_img_create(parent);
    lv_img_set_src(img_bolt, &bolt);
    lv_obj_set_pos(img_bolt, 42, 6);
    lv_obj_set_size(img_bolt, 14, 20);
    lv_obj_set_style_opa(img_bolt, 0, 0);
}

static void battery_card_create()
{
    if (card_Battery.obj == NULL || lv_obj_is_valid(card_Battery.obj) == false)
    {
        card_Battery.create(lv_layer_sys(), BATTERY_CARD_X, BATTERY_CARD_HIDE_Y, BATTERY_CARD_WIDTH, BATTERY_CARD_HEIGHT, LV_ALIGN_TOP_LEFT);
        card_Battery.show(CARD_ANIM_NONE);
        battery_card_construct(card_Battery.obj);
    }
}

void battery_card_check()
{
    static int cnt = 0;
    static bool last_charging = false;
    if (current_mode == MODE_MAINMENU)
    {
        if (expanded == false)
        {
            expanded = true;
            LOCKLV();
            battery_card_create();
            card_Battery.move(BATTERY_CARD_X, BATTERY_CARD_SHOW_Y);
            UNLOCKLV();
            cnt = 20;
        }
        ++cnt;
        if (cnt >= 20)
        {
            int16_t voltage = PowerManager_getBatteryVoltage();
            bool charging = PowerManager_isCharging();
            if (voltage > 0)
            {
                int percent = battery_percent_from_voltage(voltage);
                lv_color_t color = charging ? lv_color_hex(0x34C759) :
                                   (percent < 20 ? lv_color_hex(0xFF3B30) : lv_color_white());
                lv_color_t text_color = (charging || percent < 20) ? lv_color_white() : lv_color_black();
                LOCKLV();
                lv_label_set_text_fmt(battery_label, "%d", percent);
                lv_obj_set_width(battery_fill, 26 * percent / 100);
                lv_obj_set_style_border_color(battery_body, color, 0);
                lv_obj_set_style_bg_color(battery_fill, color, 0);
                lv_obj_set_style_bg_color(battery_cap, color, 0);
                lv_obj_set_style_text_color(battery_label, text_color, 0);
                UNLOCKLV();
            }
            if (charging != last_charging)
            {
                last_charging = charging;
                if (charging)
                {
                    LOCKLV();
                    card_Battery.size(BATTERY_CARD_WIDTH_CHARGING, BATTERY_CARD_HEIGHT);
                    lv_obj_fade_in(img_bolt, 500, 0);
                    UNLOCKLV();
                }
                else
                {
                    LOCKLV();
                    card_Battery.size(BATTERY_CARD_WIDTH, BATTERY_CARD_HEIGHT);
                    lv_obj_fade_out(img_bolt, 300, 0);
                    UNLOCKLV();
                }
            }
            cnt = 0;
        }
    }
    else
    {
        if (expanded)
        {
            expanded = false;
            LOCKLV();
            card_Battery.move(BATTERY_CARD_X, BATTERY_CARD_HIDE_Y);
            UNLOCKLV();
        }
    }
}
