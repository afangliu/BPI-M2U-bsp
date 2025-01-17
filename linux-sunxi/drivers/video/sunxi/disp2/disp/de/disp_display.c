#include "disp_display.h"

disp_dev_t gdisp;

s32 bsp_disp_init(disp_bsp_init_para * para)
{
	u32 num_screens, disp;

	memset(&gdisp,0x00,sizeof(disp_dev_t));
	memcpy(&gdisp.init_para,para,sizeof(disp_bsp_init_para));
	para->shadow_protect = bsp_disp_shadow_protect;
	disp_init_feat(&para->feat_init);

	num_screens = bsp_disp_feat_get_num_screens();
	for (disp = 0; disp < num_screens; disp++) {
#if defined(__LINUX_PLAT__)
		spin_lock_init(&gdisp.screen[disp].flag_lock);
		tasklet_init(&gdisp.screen[disp].tasklet, disp_tasklet, (unsigned long)disp);
#endif
	}

	bsp_disp_set_print_level(DEFAULT_PRINT_LEVLE);
	disp_init_al(para);
	disp_init_lcd(para);
#if defined(SUPPORT_HDMI)
	disp_init_hdmi(para);
#endif
#if defined(SUPPORT_TV)
	disp_init_tv_para(para);
#endif

#if defined(SUPPORT_EDP)
	disp_init_edp(para);
#endif /*endif SUPPORT_EDP */

	disp_init_mgr(para);
	disp_init_enhance(para);
	disp_init_smbl(para);
	disp_init_capture(para);

#if defined(SUPPORT_WB)
	disp_init_format_convert_manager(para);
#endif
#if defined SUPPORT_EINK
	disp_init_eink(para);
#endif

	disp_init_connections(para);

	return DIS_SUCCESS;
}

s32 bsp_disp_exit(u32 mode)
{
	//u32 num_screens;

	//num_screens = bsp_disp_feat_get_num_screens();

	if (mode == DISP_EXIT_MODE_CLEAN_ALL) {
		/* close all mod and unregister all irq */

	}	else if (mode == DISP_EXIT_MODE_CLEAN_PARTLY) {
		/* unregister all irq */

	}

	return DIS_SUCCESS;
}

s32 bsp_disp_open(void)
{
	return DIS_SUCCESS;
}

s32 bsp_disp_close(void)
{
	return DIS_SUCCESS;
}

s32 disp_device_attached(int disp_mgr, int disp_dev,
			struct disp_device_config *config)
{
	struct disp_manager *mgr = NULL;
	struct disp_device *dispdev = NULL;

	mgr = disp_get_layer_manager(disp_mgr);
	if (!mgr)
		return -1;

	/* no need to attch */
	if (mgr->device && (config->type == mgr->device->type))
		return 0;

	/* detach manager and device first */
	if (mgr->device) {
		dispdev = mgr->device;
		if (dispdev->is_enabled && dispdev->is_enabled(dispdev)
			&& dispdev->disable)
			dispdev->disable(dispdev);
		if (dispdev->unset_manager)
			dispdev->unset_manager(dispdev);
	}

	dispdev = disp_device_get(disp_dev, config->type);
	if (dispdev && dispdev->set_manager) {
			dispdev->set_manager(dispdev, mgr);
			DE_WRN("attched ok, mgr%d<-->device%d, type=%d\n",
					disp_mgr, disp_dev, (u32)config->type);
			if (dispdev->set_static_config)
				dispdev->set_static_config(dispdev, config);
			return 0;
	}

	return -1;
}

s32 disp_device_attached_and_enable(int disp_mgr, int disp_dev,
				    struct disp_device_config *config)
{
	struct disp_manager *mgr = NULL;
	struct disp_device *dispdev = NULL;
	int ret = 0;

	mgr = disp_get_layer_manager(disp_mgr);
	if (!mgr)
		return -1;

	if (mgr->device && mgr->device->type != config->type) {
		if (mgr->device->is_enabled(mgr->device))
			mgr->device->disable(mgr->device);

		if (mgr->device->unset_manager)
			mgr->device->unset_manager(mgr->device);
	}

	if ((!mgr->device) && (config->type != DISP_OUTPUT_TYPE_NONE)) {
		dispdev = disp_device_get(disp_dev, config->type);
		if (dispdev && dispdev->set_manager) {
			dispdev->set_manager(dispdev, mgr);
		} else {
			ret = -1;
			goto exit;
		}
	}

	if (mgr->device) {
		bool update = true;

		if (mgr->device->check_config_dirty)
			update = mgr->device->check_config_dirty(mgr->device,
								 config);

		if (update) {
			static char const *fmt_name[] = {
				"rgb",
				"yuv444",
				"yuv422",
				"yuv420"
			};
			static char const *bits_name[] = {
				"8bits",
				"10bits",
				"12bits",
				"16bits"
			};
			if (mgr->device->is_enabled(mgr->device))
				mgr->device->disable(mgr->device);

			if (mgr->device->set_static_config)
				ret = mgr->device->set_static_config(mgr->device,
							config);
			if (ret != 0)
				goto exit;

			if (config->type == DISP_OUTPUT_TYPE_TV)
				disp_delay_ms(300);
			ret = mgr->device->enable(mgr->device);
			DE_WRN("attched %s, mgr%d<-->dev%d\n",
				(ret == 0) ? "ok" : "fail",
				disp_mgr, disp_dev);
			DE_WRN("type,mode,fmt,bits,eotf,cs=%d,%d,%s,%s,%d,%d\n",
				config->type,
				config->mode,
				(config->format < 4) ?
				    fmt_name[config->format] : "undef",
				(config->bits < 4) ?
				    bits_name[config->bits] : "undef",
				config->eotf,
				config->cs);
			if (ret != 0)
				goto exit;

		}
	}

	return 0;

exit:
	return ret;
}


s32 disp_device_detach(int disp_mgr, int disp_dev, enum disp_output_type output_type)
{
	struct disp_manager *mgr = NULL;
	struct disp_device *dispdev = NULL;

	mgr = disp_get_layer_manager(disp_mgr);
	if (!mgr) {
		DE_WRN("get mgr%d fail\n", disp_mgr);
		return -1;
	}

	dispdev = disp_device_get(disp_dev, output_type);
	if (!dispdev) {
		DE_WRN("get device%d fail\n", disp_dev);
		return -1;
	}

	if (mgr->device == dispdev) {
		if (dispdev->disable)
			dispdev->disable(dispdev);
		if (dispdev->unset_manager)
			dispdev->unset_manager(dispdev);
	}

	return 0;
}

s32 bsp_disp_device_switch(int disp, enum disp_output_type output_type, enum disp_output_type mode)
{
	int num_screens = 0;
	int disp_dev;
	int ret = -1;
	struct disp_device_config config;

	config.type = output_type;
	config.mode = mode;
	config.format = (output_type == DISP_OUTPUT_TYPE_LCD) ?
			DISP_CSC_TYPE_RGB : DISP_CSC_TYPE_YUV444;
	config.bits = DISP_DATA_8BITS;
	config.eotf = DISP_EOTF_GAMMA22;
	config.cs = DISP_UNDEF;

	ret = disp_device_attached_and_enable(disp, disp, &config);
	if (0 != ret) {
		num_screens = bsp_disp_feat_get_num_screens();
		for (disp_dev = 0; disp_dev < num_screens; disp_dev++) {
			ret = disp_device_attached_and_enable(disp,
							      disp_dev,
							      &config);
			if (0 == ret)
				break;
		}
	}

	return ret;
}

s32 bsp_disp_device_set_config(int disp, struct disp_device_config *config)
{
	int num_screens = 0;
	int disp_dev;
	int ret = -1;

	ret = disp_device_attached_and_enable(disp, disp, config);
	if (0 != ret) {
		num_screens = bsp_disp_feat_get_num_screens();
		for (disp_dev=0; disp_dev<num_screens; disp_dev++) {
			ret = disp_device_attached_and_enable(disp,
							      disp_dev,
							      config);
			if (0 == ret)
				break;
		}
	}

	return ret;
}

s32 bsp_disp_eink_update(struct disp_eink_manager *manager,
			struct disp_layer_config *config,
			unsigned int layer_num,
			enum eink_update_mode mode,
			struct area_info *update_area)
{
	int ret = -1;
	struct area_info area;

	memcpy(&area, update_area, sizeof(struct area_info));

	if (manager)
		ret = manager->eink_update(manager, config, layer_num,
						mode, area);
	else
		__debug("eink manager is NULL!\n");

	return ret;
}


s32 bsp_disp_eink_set_temperature(struct disp_eink_manager* manager, unsigned int temp)
{
	s32 ret = -1;
	if (manager)
		ret = manager->set_temperature(manager, temp);
	else
		pr_err("eink manager is NULL!\n");

	return ret;
}


s32 bsp_disp_eink_get_temperature(struct disp_eink_manager* manager)
{
	s32 ret = -1;
	if (manager)
		ret = manager->get_temperature(manager);
	else
		pr_err("eink manager is NULL!\n");

	return ret;
}

s32 bsp_disp_eink_op_skip(struct disp_eink_manager *manager, unsigned int skip)
{
	s32 ret = -1;
	if (manager)
		ret = manager->op_skip(manager, skip);
	else
		pr_err("eink manager is NULL!\n");

	return ret;
}

s32 disp_init_connections(disp_bsp_init_para * para)
{
	u32 disp = 0;
	u32 num_screens = 0;
	u32 num_layers = 0,layer_id = 0;
	u32 i = 0;

	DE_INF("disp_init_connections\n");

	num_screens = bsp_disp_feat_get_num_screens();
	for (disp=0; disp<num_screens; disp++) {
		struct disp_manager *mgr;
		struct disp_layer *lyr;
		struct disp_device *dispdev = NULL;
		struct disp_enhance *enhance = NULL;
		struct disp_smbl *smbl = NULL;
		struct disp_capture *cptr = NULL;

		mgr = disp_get_layer_manager(disp);
		if (!mgr)
			continue;

		/* connect layer & it's manager */
		num_layers = bsp_disp_feat_get_num_layers(disp);
		for (layer_id=0; layer_id<num_layers; layer_id++) {
			lyr = disp_get_layer_1(disp, layer_id);
			if (NULL != lyr) {
				lyr->set_manager(lyr, mgr);
			}
		}

		if ((1 == para->boot_info.sync) && (disp == para->boot_info.disp)
			&& (DISP_OUTPUT_TYPE_LCD == para->boot_info.type)) {
			/* connect device & it's manager */
			dispdev = disp_device_get(disp, DISP_OUTPUT_TYPE_LCD);
			if ((dispdev) && (dispdev->set_manager)) {
				dispdev->set_manager(dispdev, mgr);
			} else {
				for (i=0; i<num_screens; i++) {
					dispdev = disp_device_get(i, DISP_OUTPUT_TYPE_LCD);
					if ((dispdev) && (dispdev->set_manager))
						dispdev->set_manager(dispdev, mgr);
				}
			}
		} else if (0 == para->boot_info.sync) {
			dispdev = disp_device_get(disp, DISP_OUTPUT_TYPE_LCD);
			if ((dispdev) && (dispdev->set_manager))
				dispdev->set_manager(dispdev, mgr);
		}

		enhance = disp_get_enhance(disp);
		if (enhance && (enhance->set_manager)) {
			enhance->set_manager(enhance, mgr);
		}

		smbl = disp_get_smbl(disp);
		if (smbl && (smbl->set_manager)) {
			smbl->set_manager(smbl, mgr);
		}

		cptr = disp_get_capture(disp);
		if (cptr && (cptr->set_manager)) {
			cptr->set_manager(cptr, mgr);
		}
	}

	return 0;
}

s32 bsp_disp_sync_with_hw(disp_bsp_init_para * para)
{
	if ((1 == para->boot_info.sync)
		&& (DISP_OUTPUT_TYPE_NONE != para->boot_info.type)) {
		int num_screens = 0;
		int disp = para->boot_info.disp;
		int disp_dev = disp;
		enum disp_output_type type =
			(enum disp_output_type)para->boot_info.type;
		enum disp_tv_mode mode =
			(enum disp_tv_mode)para->boot_info.mode;
		enum disp_csc_type format =
			(enum disp_csc_type)para->boot_info.format;
		enum disp_data_bits bits =
			(enum disp_data_bits)para->boot_info.bits;
		enum disp_color_space cs =
			(enum disp_color_space)para->boot_info.cs;
		enum disp_eotf eotf = (enum disp_eotf)para->boot_info.eotf;
		int ret = -1;
		struct disp_manager *mgr = NULL;

		struct disp_device_config config;

		config.type = type;
		config.mode = mode;
		config.format = format;
		config.bits = bits;
		config.eotf = eotf;
		config.cs = cs;

		mgr = disp_get_layer_manager(disp);
		if (!mgr)
			return -1;

		/* attach manager and display device */
		ret = disp_device_attached(disp, disp_dev, &config);
		if (0 != ret) {
			num_screens = bsp_disp_feat_get_num_screens();
			for (disp_dev=0; disp_dev<num_screens; disp_dev++) {
				ret = disp_device_attached(disp, disp_dev,
								&config);
				if (0 == ret)
					break;
			}
		}

		/* enable display device(only software) */
		if (0 != ret) {
			/* attach fail */
			DE_WRN("something error, can't find device(%d) for manager %d\n",
				(int)type, disp);
			return -1;
		}
		if (mgr->device && mgr->device->sw_enable) {
			if (mgr->device->set_static_config)
				mgr->device->set_static_config(mgr->device,
							&config);
			return mgr->device->sw_enable(mgr->device);
		}
	}

	return -1;
}
/***********************************************************
 *
 * interrupt proc
 *
 ***********************************************************/
static s32 bsp_disp_cfg_get(u32 disp)
{
	return gdisp.screen[disp].cfg_cnt;
}

s32 bsp_disp_shadow_protect(u32 disp, bool protect)
{
	s32 ret = -1;
	u32 cnt = 0;
	u32 max_cnt = 50;
	u32 delay = 10;//us
	//struct disp_video_timings tt;
	struct disp_manager *mgr = NULL;
#ifdef __LINUX_PLAT__
	unsigned long flags;
#endif
	__inf("sel=%d, protect:%d,  cnt=%d\n", disp, protect, gdisp.screen[disp].cfg_cnt);

	mgr = disp_get_layer_manager(disp);
	if (mgr && mgr->device) {
		if (DISP_OUTPUT_TYPE_LCD == mgr->device->type) {
			//FIXME
		} else if (DISP_OUTPUT_TYPE_HDMI == mgr->device->type) {
			//FIXME
		}
	}

	if (protect) {
		while ((0 != ret) && (cnt < max_cnt)) {
#ifdef __LINUX_PLAT__
			spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
			cnt ++;
			if (gdisp.screen[disp].have_cfg_reg == false) {
				gdisp.screen[disp].cfg_cnt++;
				ret = 0;
			}
#ifdef __LINUX_PLAT__
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
			if (0 != ret)
				disp_delay_us(delay);
		}

		if (0 != ret) {
			DE_INF("wait for reg load finish time out\n");
#if defined(__LINUX_PLAT__)
			spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
			gdisp.screen[disp].cfg_cnt++;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
		}
	} else {
#if defined(__LINUX_PLAT__)
			spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
			gdisp.screen[disp].cfg_cnt--;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
	}
	__inf("sel=%d, protect:%d,  cnt=%d\n", disp, protect, gdisp.screen[disp].cfg_cnt);
	return DIS_SUCCESS;
}

s32 bsp_disp_vsync_event_enable(u32 disp, bool enable)
{
	gdisp.screen[disp].vsync_event_en = enable;

	return DIS_SUCCESS;
}

static s32 disp_sync_all(u32 disp, bool sync)
{
	struct disp_manager *mgr;
	struct disp_device *dispdev;

	mgr = disp_get_layer_manager(disp);
	if (!mgr) {
		DE_WRN("get mgr%d fail\n", disp);
	} else {
		dispdev = mgr->device;
		if (mgr->sync)
			mgr->sync(mgr, sync);
		if (dispdev && dispdev->get_status) {
			if (0 != dispdev->get_status(dispdev))
				gdisp.screen[disp].health_info.error_cnt ++;
		}
	}

	return 0;
}

#if defined(__LINUX_PLAT__)
//return 10fps
s32 bsp_disp_get_fps(u32 disp)
{
	u32 pre_time_index, cur_time_index;
	u32 pre_time, cur_time;
	u32 fps = 0xff;

	pre_time_index = gdisp.screen[disp].health_info.sync_time_index;
	cur_time_index = (pre_time_index == 0)? (DEBUG_TIME_SIZE -1):(pre_time_index-1);

	pre_time = gdisp.screen[disp].health_info.sync_time[pre_time_index];
	cur_time = gdisp.screen[disp].health_info.sync_time[cur_time_index];

	if (pre_time != cur_time) {
		fps = 1000 * 100 / (cur_time - pre_time);
	}

	return fps;
}

static void disp_sync_checkin(u32 disp)
{
	u32 index = gdisp.screen[disp].health_info.sync_time_index;
	gdisp.screen[disp].health_info.sync_time[index] = jiffies;
	index ++;
	index = (index>=DEBUG_TIME_SIZE)?0:index;
	gdisp.screen[disp].health_info.sync_time_index = index;
}

s32 bsp_disp_get_health_info(u32 disp, disp_health_info *info)
{
	if (info)
		memcpy(info, &gdisp.screen[disp].health_info, sizeof(disp_health_info));
	return 0;
}

#endif

void sync_event_proc(u32 disp, bool timeout)
{
	int ret;
#if defined(__LINUX_PLAT__)
	unsigned long flags;

	gdisp.screen[disp].health_info.irq_cnt ++;
#endif

	if (!timeout) {

#if defined(__LINUX_PLAT__)
		spin_lock_irqsave(&gdisp.screen[disp].flag_lock, flags);
#endif
		if (0 == bsp_disp_cfg_get(disp)) {
			gdisp.screen[disp].have_cfg_reg = true;
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif

			disp_sync_all(disp, true);
			if (gdisp.init_para.disp_int_process)
				gdisp.init_para.disp_int_process(disp);

			gdisp.screen[disp].have_cfg_reg = false;

		} else {
#if defined(__LINUX_PLAT__)
			spin_unlock_irqrestore(&gdisp.screen[disp].flag_lock, flags);
#endif
			disp_sync_all(disp, false);
			gdisp.screen[disp].health_info.skip_cnt++;
		}
	} else {
		disp_sync_all(disp, false);
		gdisp.screen[disp].health_info.skip_cnt_timeout++;
	}

	if (gdisp.screen[disp].vsync_event_en && gdisp.init_para.vsync_event) {
		ret = gdisp.init_para.vsync_event(disp);
		if (ret == 0)
			gdisp.screen[disp].health_info.vsync_cnt++;
		else
			gdisp.screen[disp].health_info.vsync_skip_cnt++;
	}
#if defined(__LINUX_PLAT__)
	tasklet_schedule(&gdisp.screen[disp].tasklet);
#endif
	disp_sync_checkin(disp);

	return ;
}

s32 bsp_disp_get_output_type(u32 disp)
{
	struct disp_manager *mgr = disp_get_layer_manager(disp);
	if (mgr) {
		struct disp_device *dispdev = mgr->device;
		if (dispdev && dispdev->is_enabled && dispdev->is_enabled(dispdev))
			return dispdev->type;
	}

	return DISP_OUTPUT_TYPE_NONE;
}

void disp_tasklet(unsigned long data)
{
	struct disp_manager *mgr;
	u32 disp = (u32)data;

	mgr = disp_get_layer_manager(disp);
	if (!mgr) {
		DE_WRN("get mgr%d fail\n", disp);
	} else {
		if (mgr->tasklet)
			mgr->tasklet(mgr);
	}
}

#ifdef CONFIG_DEVFREQ_DRAM_FREQ_IN_VSYNC
s32 bsp_disp_get_vb_time(void)
{
	u32 num_screens, screen_id;
	struct disp_video_timings tt;
	u32 vb_time = 0;

	num_screens = bsp_disp_feat_get_num_screens();
	for (screen_id=0; screen_id<num_screens; screen_id++) {
		if (bsp_disp_get_output_type(screen_id) == DISP_OUTPUT_TYPE_LCD) {
			struct disp_device* lcd;
			u32 time_per_line = 0;
			u32 start_delay = 0;

			lcd = disp_get_lcd(screen_id);
			if (!lcd) {
				DE_WRN("get lcd%d fail\n", screen_id);
			}

			if (lcd && lcd->get_timings) {
				u32 fps = 0;

				lcd->get_timings(lcd, &tt);
				if ((tt.ver_total_time != 0) && (tt.hor_total_time != 0))
					fps = tt.pixel_clk*1000/(tt.ver_total_time*tt.hor_total_time);
				start_delay = tt.ver_total_time - tt.y_res - 10;
				fps = (0 == fps)? 60:fps;
				time_per_line = 1000000 / fps / tt.ver_total_time;
				vb_time = (start_delay) * time_per_line;

			} else if (bsp_disp_get_output_type(screen_id) == DISP_OUTPUT_TYPE_HDMI) {
				/* FIXME: add hdmi */
			}
		}
	}
	return vb_time;
}

/* returns: us */
s32 bsp_disp_get_next_vb_time(void)
{
	u32 cur_line;
	u32 num_screens, screen_id;
	struct disp_video_timings tt;
	u32 next_time = 16000;

	num_screens = bsp_disp_feat_get_num_screens();
	for (screen_id=0; screen_id<num_screens; screen_id++) {
		if (bsp_disp_get_output_type(screen_id) == DISP_OUTPUT_TYPE_LCD) {
			struct disp_device* lcd;
			u32 time_per_line = 0;
			disp_panel_para info;

			memset(&info, 0, sizeof(disp_panel_para));
			lcd = disp_get_lcd(screen_id);
			if (lcd && lcd->get_panel_info)
				lcd->get_panel_info(lcd, &info);
			cur_line = disp_al_lcd_get_cur_line(screen_id, &info);
			if (!lcd) {
				DE_WRN("get lcd%d fail\n", screen_id);
			}
			if (LCD_IF_EDP != info.lcd_if) {
				if (lcd && lcd->get_timings) {
					u32 fps = 0;

					lcd->get_timings(lcd, &tt);
					if ((tt.ver_total_time != 0) && (tt.hor_total_time != 0))
						fps = tt.pixel_clk*1000/(tt.ver_total_time*tt.hor_total_time);
					fps = (0 == fps)? 60:fps;
					time_per_line = 1000000 / fps / tt.ver_total_time;
					next_time = (tt.ver_total_time - cur_line) * time_per_line;
				}
			}
		} else if (bsp_disp_get_output_type(screen_id) == DISP_OUTPUT_TYPE_HDMI) {
			/* FIXME: add hdmi */
		}
	}
	return next_time;
}

s32 bsp_disp_is_in_vb(void)
{
	u32 num_screens, screen_id;
	s32 ret = 1;

	num_screens = bsp_disp_feat_get_num_screens();
	for (screen_id=0; screen_id<num_screens; screen_id++) {
		if (bsp_disp_get_output_type(screen_id) == DISP_OUTPUT_TYPE_LCD) {
			struct disp_device* lcd;
			disp_panel_para info;

			lcd = disp_get_lcd(screen_id);
			if (!lcd) {
				DE_WRN("get lcd%d fail\n", screen_id);
			}
			memset(&info, 0, sizeof(disp_panel_para));
			if (lcd && lcd->get_panel_info)
				lcd->get_panel_info(lcd, &info);
			ret = disp_al_lcd_query_irq(screen_id, LCD_IRQ_TCON0_VBLK, &info);
		} else if (bsp_disp_get_output_type(screen_id) == DISP_OUTPUT_TYPE_HDMI) {
			/* FIXME: add hdmi */
		}
	}
	return ret;
}
#endif

s32 bsp_disp_set_print_level(u32 print_level)
{
	gdisp.print_level = print_level;

	return 0;
}

s32 bsp_disp_get_print_level(void)
{
	return gdisp.print_level;
}

s32 bsp_disp_get_screen_physical_width(u32 disp)
{
	s32 width = 0, height = 0;
	struct disp_manager *mgr = NULL;

	mgr = disp_get_layer_manager(disp);
	if (mgr && mgr->device && mgr->device->get_dimensions) {
		mgr->device->get_dimensions(mgr->device, &width, &height);
	}

	return width;
}

s32 bsp_disp_get_screen_physical_height(u32 disp)
{
	s32 width = 0, height = 0;
	struct disp_manager *mgr = NULL;

	mgr = disp_get_layer_manager(disp);
	if (mgr && mgr->device && mgr->device->get_dimensions) {
		mgr->device->get_dimensions(mgr->device, &width, &height);
	}

	return height;
}

s32 bsp_disp_get_screen_width(u32 disp)
{
	s32 width = 0;
	//FIXME
	return width;
}

s32 bsp_disp_get_screen_height(u32 disp)
{
	s32 height = 0;
	//FIXME

	return height;
}


s32 bsp_disp_get_screen_width_from_output_type(u32 disp, u32 output_type,
						u32 output_mode)
{
	u32 width = 800, height = 480;
	struct disp_device *dispdev;

	if (DISP_OUTPUT_TYPE_LCD == output_type) {
		struct disp_manager *mgr;

		mgr = disp_get_layer_manager(disp);
		if (mgr && mgr->device && mgr->device->get_resolution) {
			mgr->device->get_resolution(mgr->device, &width,
						    &height);
		}
	} else if (DISP_OUTPUT_TYPE_EDP == output_type) {
		dispdev = disp_device_get(disp, DISP_OUTPUT_TYPE_EDP);
		if (dispdev)
			dispdev->get_resolution(dispdev, &width, &height);
	} else if ((DISP_OUTPUT_TYPE_HDMI == output_type)
			|| (DISP_OUTPUT_TYPE_TV == output_type)
			|| (DISP_OUTPUT_TYPE_VGA == output_type)) {
		switch(output_mode) {
		case DISP_TV_MOD_NTSC:
		case DISP_TV_MOD_480I:
		case DISP_TV_MOD_480P:
			width = 720;
			height = 480;
			break;
		case DISP_TV_MOD_PAL:
		case DISP_TV_MOD_576I:
		case DISP_TV_MOD_576P:
			width = 720;
			height = 576;
			break;
		case DISP_TV_MOD_720P_50HZ:
		case DISP_TV_MOD_720P_60HZ:
			width = 1280;
			height = 720;
			break;
		case DISP_TV_MOD_1080P_50HZ:
		case DISP_TV_MOD_1080P_60HZ:
		case DISP_TV_MOD_1080P_30HZ:
		case DISP_TV_MOD_1080P_25HZ:
		case DISP_TV_MOD_1080P_24HZ:
		case DISP_TV_MOD_1080I_50HZ:
		case DISP_TV_MOD_1080I_60HZ:
			width = 1920;
			height = 1080;
			break;
		case DISP_TV_MOD_3840_2160P_30HZ:
		case DISP_TV_MOD_3840_2160P_25HZ:
		case DISP_TV_MOD_3840_2160P_24HZ:
			width = 3840;
			height = 2160;
			break;
		case DISP_TV_MOD_4096_2160P_24HZ:
			width = 4096;
			height = 2160;
			break;
		case DISP_VGA_MOD_800_600P_60:
			width = 800;
			height = 600;
			break;
		case DISP_VGA_MOD_1024_768P_60:
			width = 1024;
			height = 768;
			break;
		case DISP_VGA_MOD_1280_768P_60:
			width = 1280;
			height = 768;
			break;
		case DISP_VGA_MOD_1280_800P_60:
			width = 1280;
			height = 800;
			break;
		case DISP_VGA_MOD_1366_768P_60:
			width = 1366;
			height = 768;
			break;
		case DISP_VGA_MOD_1440_900P_60:
			width = 1440;
			height = 900;
			break;
		case DISP_VGA_MOD_1920_1080P_60:
			width = 1920;
			height = 1080;
			break;
		case DISP_VGA_MOD_1920_1200P_60:
			width = 1920;
			height = 1200;
			break;
		case DISP_VGA_MOD_1280_720P_60:
			width = 1280;
			height = 720;
			break;
		case DISP_TV_MOD_1280_1024P_60HZ:
			width = 1280;
			height = 1024;
			break;
		case DISP_TV_MOD_1024_768P_60HZ:
				width = 1024;
			height = 768;
			break;
		case DISP_TV_MOD_900_540P_60HZ:
			width = 900;
			height = 540;
			break;
		}
	}
	/* FIXME: add other output device res */

	return width;
}

s32 bsp_disp_get_screen_height_from_output_type(u32 disp, u32 output_type, u32 output_mode)
{
	u32 width = 800, height = 480;
	struct disp_device *dispdev;

	if (DISP_OUTPUT_TYPE_LCD == output_type) {
		struct disp_manager *mgr;
		mgr = disp_get_layer_manager(disp);
		if (mgr && mgr->device && mgr->device->get_resolution) {
			mgr->device->get_resolution(mgr->device, &width,
						    &height);
		}
	} else if (DISP_OUTPUT_TYPE_EDP == output_type) {
		dispdev = disp_device_get(disp, DISP_OUTPUT_TYPE_EDP);
		if (dispdev)
			dispdev->get_resolution(dispdev, &width, &height);
	} else if ((DISP_OUTPUT_TYPE_HDMI == output_type)
			|| (DISP_OUTPUT_TYPE_TV == output_type)
			|| (DISP_OUTPUT_TYPE_VGA == output_type)) {
		switch(output_mode) {
		case DISP_TV_MOD_NTSC:
		case DISP_TV_MOD_480I:
		case DISP_TV_MOD_480P:
			width = 720;
			height = 480;
			break;
		case DISP_TV_MOD_PAL:
		case DISP_TV_MOD_576I:
		case DISP_TV_MOD_576P:
			width = 720;
			height = 576;
			break;
		case DISP_TV_MOD_720P_50HZ:
		case DISP_TV_MOD_720P_60HZ:
			width = 1280;
			height = 720;
			break;
		case DISP_TV_MOD_1080P_50HZ:
		case DISP_TV_MOD_1080P_60HZ:
		case DISP_TV_MOD_1080P_30HZ:
		case DISP_TV_MOD_1080P_25HZ:
		case DISP_TV_MOD_1080P_24HZ:
		case DISP_TV_MOD_1080I_50HZ:
		case DISP_TV_MOD_1080I_60HZ:
			width = 1920;
			height = 1080;
			break;
		case DISP_TV_MOD_3840_2160P_30HZ:
		case DISP_TV_MOD_3840_2160P_25HZ:
		case DISP_TV_MOD_3840_2160P_24HZ:
			width = 3840;
			height = 2160;
			break;
		case DISP_TV_MOD_4096_2160P_24HZ:
			width = 4096;
			height = 2160;
			break;
		case DISP_VGA_MOD_800_600P_60:
			width = 800;
			height = 600;
			break;
		case DISP_VGA_MOD_1024_768P_60:
			width = 1024;
			height = 768;
			break;
		case DISP_VGA_MOD_1280_768P_60:
			width = 1280;
			height = 768;
			break;
		case DISP_VGA_MOD_1280_800P_60:
			width = 1280;
			height = 800;
			break;
		case DISP_VGA_MOD_1366_768P_60:
			width = 1366;
			height = 768;
			break;
		case DISP_VGA_MOD_1440_900P_60:
			width = 1440;
			height = 900;
			break;
		case DISP_VGA_MOD_1920_1080P_60:
			width = 1920;
			height = 1080;
			break;
		case DISP_VGA_MOD_1920_1200P_60:
			width = 1920;
			height = 1200;
			break;
		case DISP_VGA_MOD_1280_720P_60:
			width = 1280;
			height = 720;
			break;
		case DISP_TV_MOD_1280_1024P_60HZ:
			width = 1280;
			height = 1024;
			break;
		case DISP_TV_MOD_1024_768P_60HZ:
			width = 1024;
			height = 768;
			break;
		case DISP_TV_MOD_900_540P_60HZ:
			width = 900;
			height = 540;
			break;
		}
	}
	/* FIXME: add other output device res */

	return height;
}

s32 bsp_disp_set_hdmi_func(struct disp_device_func * func)
{
		u32 disp = 0;
	u32 num_screens = 0;
	s32 ret = 0, registered_cnt = 0;

	num_screens = bsp_disp_feat_get_num_screens();
	for (disp=0; disp<num_screens; disp++) {
		struct disp_device *hdmi;
		hdmi = disp_device_find(disp, DISP_OUTPUT_TYPE_HDMI);
		if (hdmi) {
			ret = hdmi->set_func(hdmi, func);
			if (0 == ret)
				registered_cnt ++;
		}
	}

	if (0 != registered_cnt) {
		DE_INF("registered!!\n");
		gdisp.hdmi_registered = 1;
		if (gdisp.init_para.start_process)
			gdisp.init_para.start_process();

		return 0;
	}

	return -1;
}

s32 bsp_disp_set_edp_func(struct disp_tv_func *func)
{
	u32 disp = 0;
	u32 num_screens = 0;
	s32 ret = 0, registered_cnt = 0;

	num_screens = bsp_disp_feat_get_num_screens();
	for (disp = 0; disp < num_screens; disp++) {
		struct disp_device *edp;

		edp = disp_device_find(disp, DISP_OUTPUT_TYPE_EDP);
		if (edp) {
			if (edp->set_tv_func)
				ret = edp->set_tv_func(edp, func);
			if (ret == 0)
				registered_cnt++;
		}
	}

	if (registered_cnt != 0) {
		DE_INF("edp registered!!\n");
		gdisp.edp_registered = 1;
		if (gdisp.init_para.start_process)
			gdisp.init_para.start_process();

		return 0;
	}

	return -1;
}

s32 bsp_disp_hdmi_check_support_mode(u32 disp, enum disp_output_type mode)
{
	u32 num_screens = 0;
	s32 ret = 0 ;

	num_screens = bsp_disp_feat_get_num_screens();
	for (disp=0; disp<num_screens; disp++) {
		struct disp_device *hdmi;
		hdmi = disp_device_find(disp, DISP_OUTPUT_TYPE_HDMI);
		if (hdmi && hdmi->check_support_mode) {
			ret = hdmi->check_support_mode(hdmi, (u32)mode);
			break;
		}
	}

	return ret;
}

s32 bsp_disp_hdmi_set_detect(bool hpd)
{
	u32 num_screens = 0;
	u32 disp;
	s32 ret = 0 ;

	num_screens = bsp_disp_feat_get_num_screens();
	for (disp=0; disp<num_screens; disp++) {
		struct disp_device *hdmi;
		hdmi = disp_device_find(disp, DISP_OUTPUT_TYPE_HDMI);
		if (hdmi && hdmi->set_detect) {
			ret = hdmi->set_detect(hdmi, hpd);
			break;
		}
	}

	return ret;
}

s32 bsp_disp_tv_set_hpd(u32 state)
{
#if defined SUPPORT_TV

	u32 disp = 0, num_screens = 0;
	s32 ret = 0;
	struct disp_device*  ptv = NULL;
	num_screens = bsp_disp_feat_get_num_screens();
	for (disp=0; disp<num_screens; disp++) {
		ptv = disp_device_find(disp, DISP_OUTPUT_TYPE_TV);
		if (ptv) {
			ret = disp_tv_set_hpd(ptv, state);
		}
		else {
			ret &= ret;
			//DE_WRN("'ptv is null\n");
			continue;
		}
	}

	if (0 != ret) {
		DE_WRN("'tv set hpd is fail!\n");
		return -1;
	}

#endif

	return 0;
}

s32 bsp_disp_tv_register(struct disp_tv_func * func)
{
#if defined SUPPORT_TV
	u32 disp = 0;
	u32 num_screens = 0;
	s32 ret = 0, registered_cnt = 0;
	struct disp_device* dispdev = NULL;

	num_screens = bsp_disp_feat_get_num_screens();
	disp_init_tv();
	for (disp=0; disp<num_screens; disp++) {
		dispdev = disp_device_find(disp, DISP_OUTPUT_TYPE_TV);
		if (dispdev && dispdev->set_tv_func) {
			ret = dispdev->set_tv_func(dispdev, func);
			if (0 == ret)
				registered_cnt ++;
		}
	}

#if defined(SUPPORT_VGA)
	disp_init_vga();
	for (disp=0; disp<num_screens; disp++) {
		dispdev = disp_device_find(disp, DISP_OUTPUT_TYPE_VGA);
		if (dispdev && dispdev->set_tv_func) {
			ret = dispdev->set_tv_func(dispdev, func);
			if (0 == ret)
				registered_cnt ++;
		}
	}
#endif
	if (0 != registered_cnt) {
		gdisp.tv_registered = 1;
		if (gdisp.init_para.start_process) {
			gdisp.init_para.start_process();
		}
		return 0;
	}
#endif
	return 0;
}

s32 bsp_disp_lcd_set_panel_funs(char *name, disp_lcd_panel_fun * lcd_cfg)
{
	struct disp_device* lcd;
	u32 num_screens;
	u32 screen_id;
	u32 registered_cnt = 0;

	num_screens = bsp_disp_feat_get_num_screens();
	for (screen_id=0; screen_id<num_screens; screen_id++) {
		lcd = disp_get_lcd(screen_id);
		if (lcd && (lcd->set_panel_func)) {
			if (!lcd->set_panel_func(lcd, name, lcd_cfg)) {
				gdisp.lcd_registered[screen_id] = 1;
				registered_cnt ++;
				DE_INF("panel driver %s register\n", name);
			}
		}
	}

	return 0;
}

void LCD_OPEN_FUNC(u32 disp, LCD_FUNC func, u32 delay)
{
	struct disp_device* lcd;

	lcd = disp_get_lcd(disp);

	if (lcd && lcd->set_open_func) {
		lcd->set_open_func(lcd, func, delay);
	}
}

void LCD_CLOSE_FUNC(u32 disp, LCD_FUNC func, u32 delay)
{
	struct disp_device* lcd;

	lcd = disp_get_lcd(disp);

	if (lcd && lcd->set_close_func) {
		lcd->set_close_func(lcd, func, delay);
	}
}

s32 bsp_disp_get_lcd_registered(u32 disp)
{
	return gdisp.lcd_registered[disp];
}

s32 bsp_disp_get_hdmi_registered(void)
{
	return gdisp.hdmi_registered;
}

s32 bsp_disp_get_tv_registered(void)
{
	return gdisp.tv_registered;
}

s32 bsp_disp_lcd_backlight_enable(u32 disp)
{
	s32 ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (NULL == lcd)
		return ret;

	if (lcd->backlight_enable) {
		ret = lcd->backlight_enable(lcd);
	}

	return ret;
}

s32 bsp_disp_lcd_backlight_disable(u32 disp)
{
	s32 ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (NULL == lcd)
		return ret;

	if (lcd && lcd->backlight_disable) {
		ret = lcd->backlight_disable(lcd);
	}

	return ret;
}

s32 bsp_disp_lcd_pwm_enable(u32 disp)
{
	s32 ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (NULL == lcd)
		return ret;

	if (lcd && lcd->pwm_enable) {
		ret = lcd->pwm_enable(lcd);
	}

	return ret;
}

s32 bsp_disp_lcd_pwm_disable(u32 disp)
{
	s32 ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (NULL == lcd)
		return ret;

	if (lcd && lcd->pwm_disable) {
		ret = lcd->pwm_disable(lcd);
	}

	return ret;
}

s32 bsp_disp_lcd_power_enable(u32 disp, u32 power_id)
{
	s32 ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (NULL == lcd)
		return ret;

	if (lcd && lcd->power_enable) {
		ret = lcd->power_enable(lcd, power_id);
	}

	return ret;
}

s32 bsp_disp_lcd_power_disable(u32 disp, u32 power_id)
{
	s32 ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (NULL == lcd)
		return ret;

	if (lcd && lcd->power_disable) {
		ret = lcd->power_disable(lcd, power_id);
	}

	return ret;
}

s32 bsp_disp_lcd_set_bright(u32 disp, u32 bright)
{
	s32 ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (NULL == lcd)
		return ret;

	if (lcd && lcd->set_bright) {
		ret = lcd->set_bright(lcd, bright);
	}

	return ret;
}

s32 bsp_disp_lcd_get_bright(u32 disp)
{
	u32 bright = 0;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (lcd && lcd->get_bright)
		bright = lcd->get_bright(lcd);

	return bright;
}

s32 bsp_disp_lcd_tcon_enable(u32 disp)
{
	int ret = -1;
	struct disp_device *lcd;
	struct disp_device *lcd_slave;

	disp_panel_para *panel_info =
	    kmalloc(sizeof(disp_panel_para), GFP_KERNEL | __GFP_ZERO);

	if (panel_info == NULL)
		goto OUT;

	lcd = disp_get_lcd(disp);
	if (lcd == NULL)
		goto FREE_INFO;

	if (lcd && lcd->get_panel_info)
		ret = lcd->get_panel_info(lcd, panel_info);

	if (ret != 0)
		goto FREE_INFO;

	if (panel_info->lcd_tcon_mode == DISP_TCON_SLAVE_MODE) {
		ret = 0;
		goto FREE_INFO;
	}

	if (lcd && lcd->tcon_enable)
		ret = lcd->tcon_enable(lcd);

	if (panel_info->lcd_tcon_mode <= DISP_TCON_MASTER_SYNC_EVERY_FRAME &&
	    panel_info->lcd_tcon_mode >= DISP_TCON_MASTER_SYNC_AT_FIRST_TIME) {
		lcd_slave = disp_get_lcd(panel_info->lcd_slave_tcon_num);
		if (lcd_slave == NULL)
			goto FREE_INFO;
		if (lcd_slave && lcd_slave->tcon_enable)
			ret = lcd_slave->tcon_enable(lcd_slave);
	}

FREE_INFO:
	if (panel_info != NULL)
		kfree(panel_info);
OUT:
	return ret;
}

s32 bsp_disp_lcd_tcon_disable(u32 disp)
{
	int ret = -1;
	struct disp_device *lcd;
	struct disp_device *lcd_slave;

	disp_panel_para *panel_info =
	    kmalloc(sizeof(disp_panel_para), GFP_KERNEL | __GFP_ZERO);

	if (panel_info == NULL)
		goto OUT;

	lcd = disp_get_lcd(disp);
	if (lcd == NULL)
		goto FREE_INFO;

	if (lcd && lcd->get_panel_info)
		ret = lcd->get_panel_info(lcd, panel_info);

	if (ret != 0)
		goto FREE_INFO;

	if (lcd && lcd->tcon_disable)
		ret = lcd->tcon_disable(lcd);

	if (panel_info->lcd_tcon_mode <= DISP_TCON_MASTER_SYNC_EVERY_FRAME &&
	    panel_info->lcd_tcon_mode >= DISP_TCON_MASTER_SYNC_AT_FIRST_TIME) {
		lcd_slave = disp_get_lcd(panel_info->lcd_slave_tcon_num);
		if (lcd_slave == NULL)
			goto FREE_INFO;
		if (lcd_slave && lcd_slave->tcon_disable)
			ret = lcd_slave->tcon_disable(lcd_slave);
	}

FREE_INFO:
	if (panel_info != NULL)
		kfree(panel_info);
OUT:
	return ret;
}

s32 bsp_disp_lcd_pin_cfg(u32 disp, u32 en)
{
	int ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (lcd && lcd->pin_cfg)
		ret = lcd->pin_cfg(lcd, en);

	return ret;
}

s32 bsp_disp_lcd_gpio_set_value(u32 disp, u32 io_index, u32 value)
{
	int ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (lcd && lcd->gpio_set_value)
		ret = lcd->gpio_set_value(lcd, io_index, value);

	return ret;
}

s32 bsp_disp_lcd_gpio_set_direction(u32 disp, unsigned int io_index, u32 direction)
{
	int ret = -1;
	struct disp_device *lcd;

	lcd = disp_get_lcd(disp);
	if (lcd && lcd->gpio_set_direction)
		ret = lcd->gpio_set_direction(lcd, io_index, direction);

	return ret;
}

s32 bsp_disp_get_panel_info(u32 disp, disp_panel_para *info)
{
	struct disp_device* lcd;
	lcd = disp_get_lcd(disp);
	if (!lcd)
		DE_WRN("get lcd%d fail\n", disp);

	if (lcd && lcd->get_panel_info)
		return lcd->get_panel_info(lcd, info);

	return DIS_FAIL;
}

int bsp_disp_get_fb_info(unsigned int disp, struct disp_layer_info *info)
{
	return disp_al_get_fb_info(disp, info);
}

int bsp_disp_get_display_size(u32 disp, unsigned int *width, unsigned int *height)
{
	return disp_al_get_display_size(disp, width, height);
}

#if defined(SUPPORT_DSI)
s32 bsp_disp_lcd_dsi_open(u32 disp)
{
	s32 ret = -1;
	disp_panel_para *panel_info = kmalloc(sizeof(disp_panel_para),
					      GFP_KERNEL | __GFP_ZERO);

	ret = bsp_disp_get_panel_info(disp, panel_info);
	if (ret == DIS_FAIL) {
		DE_WRN("%s:Get panel info failed\n", __func__);
		goto OUT;
	}

	if (panel_info->lcd_tcon_mode == DISP_TCON_SLAVE_MODE)
		goto OUT;

	ret = dsi_mode_switch(disp, 1);
	if (panel_info->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
	    disp + 1 < DEVICE_DSI_NUM)
		ret = dsi_mode_switch(disp + 1, 1);
	else if (panel_info->lcd_tcon_mode != DISP_TCON_NORMAL_MODE &&
		 panel_info->lcd_tcon_mode != DISP_TCON_DUAL_DSI)
		ret = dsi_mode_switch(panel_info->lcd_slave_tcon_num, 1);

OUT:
	kfree(panel_info);
	return ret;
}

s32 bsp_disp_lcd_dsi_close(u32 disp)
{
	s32 ret = -1;
	disp_panel_para *panel_info = kmalloc(sizeof(disp_panel_para),
					      GFP_KERNEL | __GFP_ZERO);

	ret = bsp_disp_get_panel_info(disp, panel_info);
	if (ret == DIS_FAIL) {
		DE_WRN("%s:Get panel info failed\n", __func__);
		goto OUT;
	}

	if (panel_info->lcd_tcon_mode == DISP_TCON_SLAVE_MODE)
		goto OUT;

	ret = dsi_mode_switch(disp, 0);
	if (panel_info->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
	    disp + 1 < DEVICE_DSI_NUM)
		ret = dsi_mode_switch(disp + 1, 0);
	else if (panel_info->lcd_tcon_mode != DISP_TCON_NORMAL_MODE &&
		 panel_info->lcd_tcon_mode != DISP_TCON_DUAL_DSI)
		ret = dsi_mode_switch(panel_info->lcd_slave_tcon_num, 0);
OUT:
	kfree(panel_info);
	return ret;
}

s32 bsp_disp_lcd_dsi_clk_enable(u32 disp, u32 en)
{
	s32 ret = -1;
	disp_panel_para *panel_info =
	    kmalloc(sizeof(disp_panel_para), GFP_KERNEL | __GFP_ZERO);

	ret = bsp_disp_get_panel_info(disp, panel_info);
	if (ret == DIS_FAIL) {
		DE_WRN("%s:Get panel info failed\n", __func__);
		goto OUT;
	}

	if (panel_info->lcd_tcon_mode == DISP_TCON_SLAVE_MODE)
		goto OUT;

	ret = dsi_clk_enable(disp, en);
	if (panel_info->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
	    disp + 1 < DEVICE_DSI_NUM)
		ret = dsi_clk_enable(disp + 1, en);
	else if (panel_info->lcd_tcon_mode != DISP_TCON_NORMAL_MODE &&
		 panel_info->lcd_tcon_mode != DISP_TCON_DUAL_DSI)
		ret = dsi_clk_enable(panel_info->lcd_slave_tcon_num, en);
OUT:
	kfree(panel_info);
	return ret;
}

s32 bsp_disp_lcd_dsi_dcs_wr(u32 disp, u8 command, u8 *para, u32 para_num)
{
	s32 ret = -1;
	disp_panel_para *panel_info =
	    kmalloc(sizeof(disp_panel_para), GFP_KERNEL | __GFP_ZERO);

	ret = bsp_disp_get_panel_info(disp, panel_info);
	if (ret == DIS_FAIL) {
		DE_WRN("%s:Get panel info failed\n", __func__);
		goto OUT;
	}

	if (panel_info->lcd_tcon_mode == DISP_TCON_SLAVE_MODE)
		goto OUT;

	ret = dsi_dcs_wr(disp, command, para, para_num);
	if (panel_info->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
	    disp + 1 < DEVICE_DSI_NUM &&
	    panel_info->lcd_dsi_port_num == DISP_LCD_DSI_SINGLE_PORT)
		ret = dsi_dcs_wr(disp + 1, command, para, para_num);
	else if (panel_info->lcd_tcon_mode != DISP_TCON_NORMAL_MODE &&
		 panel_info->lcd_tcon_mode != DISP_TCON_DUAL_DSI)
		ret = dsi_dcs_wr(panel_info->lcd_slave_tcon_num, command, para,
				 para_num);
OUT:
	kfree(panel_info);
	return ret;
}

s32 bsp_disp_lcd_dsi_gen_wr(u32 disp, u8 command, u8 *para, u32 para_num)
{
	s32 ret = -1;
	disp_panel_para *panel_info =
	    kmalloc(sizeof(disp_panel_para), GFP_KERNEL | __GFP_ZERO);

	ret = bsp_disp_get_panel_info(disp, panel_info);
	if (ret == DIS_FAIL) {
		DE_WRN("%s:Get panel info failed\n", __func__);
		goto OUT;
	}

	if (panel_info->lcd_tcon_mode == DISP_TCON_SLAVE_MODE)
		goto OUT;

	ret = dsi_gen_wr(disp, command, para, para_num);
	if (panel_info->lcd_tcon_mode == DISP_TCON_DUAL_DSI &&
	    disp + 1 < DEVICE_DSI_NUM &&
	    panel_info->lcd_dsi_port_num == DISP_LCD_DSI_SINGLE_PORT)
		ret = dsi_gen_wr(disp + 1, command, para, para_num);
	else if (panel_info->lcd_tcon_mode != DISP_TCON_NORMAL_MODE &&
		 panel_info->lcd_tcon_mode != DISP_TCON_DUAL_DSI)
		ret = dsi_gen_wr(panel_info->lcd_slave_tcon_num, command, para,
				 para_num);
OUT:
	kfree(panel_info);
	return ret;
}
#endif /*endif SUPPORT_DSI */
