// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2017-2020, The Linux Foundation. All rights reserved.
 */
#include <linux/module.h>
#include <cam_sensor_cmn_header.h>
#include "cam_sensor_util.h"
#include "cam_soc_util.h"
#include "cam_trace.h"
#include "cam_common_util.h"
#include "cam_packet_util.h"
#include "cam_sensor_dev.h"
#include "cam_req_mgr_dev.h"
#include "cam_sensor_soc.h"
#include "cam_sensor_core.h"
#if IS_ENABLED(CONFIG_PROJECT_INFO)
#include <linux/oem/project_info.h>
#endif
#include "oplus_cam_sensor_core.h"

bool is_ftm_current_test = false;

struct camera_vendor_match_tbl match_tbl[] = {
        {0x586,  "imx586", "Sony"      },
        {0x30d5, "s5k3m5", "Samsung"   },
        {0x5035, "gc5035", "Galaxyc"   },
        {0x471,  "imx471", "Sony"      },
        {0x481,  "imx481", "Sony"      },
        {0x2375, "gc2375", "Galaxyc"   },
        {0x689,  "imx689", "Sony"      },
        {0x0615, "imx615", "Sony"      },
        {0x0616, "imx616", "Sony"      },
        {0x4608, "hi846",  "Hynix"     },
        {0x5664, "ov64b",  "OmniVision"},
        {0x8054, "gc8054", "Galaxyc"   },
        {0x2b,   "ov02b10","OmniVision"},
        {0x2b04, "ov02b10","OmniVision"},
        {0x88,   "ov8856", "OmniVision"},
        {0x02,   "gc02m1b","Galaxyc"   },
        {0x686,  "imx686", "Sony"      },
        {0x789,  "imx789", "Sony"      },
        {0x766,  "imx766", "Sony"      },
        {0x841,  "ov08a10","OmniVision"},
        {0x355,  "imx355", "Sony"      },
};

struct cam_sensor_i2c_reg_setting sensor_setting;

struct cam_sensor_settings sensor_settings = {
#include "cam_sensor_settings.h"
};

#include <linux/firmware.h>
#include <linux/dma-contiguous.h>

struct cam_sensor_init_settings {
	struct cam_sensor_i2c_reg_setting_array ov64b_init_setting1;
	struct cam_sensor_i2c_reg_setting_array imx709_init_setting1;
	struct cam_sensor_i2c_reg_setting_array ov02b_init_setting1;
	struct cam_sensor_i2c_reg_setting_array imx890_init_setting1;
	struct cam_sensor_i2c_reg_setting_array imx355_init_setting1;
};

struct cam_sensor_init_settings sensor_init_settings = {
#include "cam_sensor_initsettings.h"
};


#define MAX_LENGTH 128
bool chip_version_old = FALSE;
struct sony_dfct_tbl_t sony_dfct_tbl;

int32_t oplus_cam_sensor_driver_cmd(struct cam_sensor_ctrl_t *s_ctrl,
	void *arg)
{
	int rc = 0;
	struct cam_control *cmd = (struct cam_control *)arg;
#ifdef OPLUS_FEATURE_CAMERA_COMMON
	switch (cmd->op_code) {
	case CAM_OEM_IO_CMD:{
		struct cam_oem_rw_ctl oem_ctl;
		struct camera_io_master oem_io_master_info;
		struct cam_sensor_cci_client oem_cci_client;
        struct cam_oem_i2c_reg_array *cam_regs = NULL;
		if (copy_from_user(&oem_ctl, (void __user *)cmd->handle,
			sizeof(struct cam_oem_rw_ctl))) {
			CAM_ERR(CAM_SENSOR,
					"Fail in copy oem control infomation form user data");
                      rc = -ENOMEM;
                      return rc;
		}
		if (oem_ctl.num_bytes > 0) {
			cam_regs = (struct cam_oem_i2c_reg_array *)kzalloc(
				sizeof(struct cam_oem_i2c_reg_array)*oem_ctl.num_bytes, GFP_KERNEL);
			if (!cam_regs) {
				rc = -ENOMEM;
                             CAM_ERR(CAM_SENSOR,"failed alloc cam_regs");
				return rc;
			}

			if (copy_from_user(cam_regs, u64_to_user_ptr(oem_ctl.cam_regs_ptr),
				sizeof(struct cam_oem_i2c_reg_array)*oem_ctl.num_bytes)) {
				CAM_INFO(CAM_SENSOR, "copy_from_user error!!!", oem_ctl.num_bytes);
				rc = -EFAULT;
				goto free_cam_regs;
			}
		}
		memcpy(&oem_io_master_info, &(s_ctrl->io_master_info),sizeof(struct camera_io_master));
		memcpy(&oem_cci_client, s_ctrl->io_master_info.cci_client,sizeof(struct cam_sensor_cci_client));
		oem_io_master_info.cci_client = &oem_cci_client;
		if (oem_ctl.slave_addr != 0) {
			oem_io_master_info.cci_client->sid = (oem_ctl.slave_addr >> 1);
		}

		switch (oem_ctl.cmd_code) {
        	case CAM_OEM_CMD_READ_DEV: {
			int i = 0;
			for (; i < oem_ctl.num_bytes; i++)
			{
				rc |= cam_cci_i2c_read(
					 oem_io_master_info.cci_client,
					 cam_regs[i].reg_addr,
					 &(cam_regs[i].reg_data),
					 oem_ctl.reg_addr_type,
					 oem_ctl.reg_data_type);
				CAM_INFO(CAM_SENSOR,
					"read addr:0x%x  Data:0x%x ",
					cam_regs[i].reg_addr, cam_regs[i].reg_data);
			}

			if (rc < 0) {
				CAM_ERR(CAM_SENSOR,
					"Fail oem ctl data ,slave sensor id is 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
				goto free_cam_regs;;
			}

			if (copy_to_user(u64_to_user_ptr(oem_ctl.cam_regs_ptr), cam_regs,
				sizeof(struct cam_oem_i2c_reg_array)*oem_ctl.num_bytes)) {
				CAM_ERR(CAM_SENSOR,
						"Fail oem ctl data ,slave sensor id is 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
				goto free_cam_regs;

			}
			break;
		}
		case CAM_OEM_CMD_WRITE_DEV: {
			struct cam_sensor_i2c_reg_setting write_setting;
			int i = 0;
			for (;i < oem_ctl.num_bytes; i++)
			{
				CAM_DBG(CAM_SENSOR,"Get from OEM addr: 0x%x data: 0x%x ",
								cam_regs[i].reg_addr, cam_regs[i].reg_data);
			}

			write_setting.addr_type = oem_ctl.reg_addr_type;
			write_setting.data_type = oem_ctl.reg_data_type;
			write_setting.size = oem_ctl.num_bytes;
			write_setting.reg_setting = (struct cam_sensor_i2c_reg_array*)cam_regs;

			rc = cam_cci_i2c_write_table(&oem_io_master_info,&write_setting);

			if (rc < 0){
				CAM_ERR(CAM_SENSOR,
					"Fail oem write data ,slave sensor id is 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
				goto free_cam_regs;
			}

			break;
		}

		case CAM_OEM_OIS_CALIB : {
			//rc = cam_ois_sem1215s_calibration(&oem_io_master_info);
                     //CAM_ERR(CAM_SENSOR, "ois calib failed rc:%d", rc);
			break;
		}

		default:
			CAM_ERR(CAM_SENSOR,
						"Unknow OEM cmd ,slave sensor id is 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
			break ;
		}

free_cam_regs:
		if (cam_regs != NULL) {
			kfree(cam_regs);
			cam_regs = NULL;
		}
		mutex_unlock(&(s_ctrl->cam_sensor_mutex));
		return rc;
	}

	case CAM_OEM_GET_ID : {
		if (copy_to_user((void __user *)cmd->handle,&s_ctrl->soc_info.index,
						sizeof(uint32_t))) {
			CAM_ERR(CAM_SENSOR,
					"copy camera id to user fail ");
		}
		break;
	}
	case CAM_GET_DPC_DATA: {
		CAM_INFO(CAM_SENSOR, "sony_dfct_tbl: fd_dfct_num=%d, sg_dfct_num=%d",
			sony_dfct_tbl.fd_dfct_num, sony_dfct_tbl.sg_dfct_num);
		if (copy_to_user((void __user *) cmd->handle, &sony_dfct_tbl,
			sizeof(struct  sony_dfct_tbl_t))) {
			CAM_ERR(CAM_SENSOR, "Failed Copy to User");
			rc = -EFAULT;
			return rc;
		}
	}
	break;
       default:
            CAM_ERR(CAM_SENSOR, "Invalid Opcode: %d", cmd->op_code);
			rc = -EINVAL;
       break;
       }
#endif
	return rc;
}

int cam_ftm_power_down(struct cam_sensor_ctrl_t *s_ctrl) {
    int rc = 0;
    CAM_ERR(CAM_SENSOR,"FTM stream off");
    if (s_ctrl->sensordata->slave_info.sensor_id == 0x586 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x30d5||
        s_ctrl->sensordata->slave_info.sensor_id == 0x471 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x481 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x5035||
        s_ctrl->sensordata->slave_info.sensor_id == 0x689 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x2375||
        s_ctrl->sensordata->slave_info.sensor_id == 0x4608||
        s_ctrl->sensordata->slave_info.sensor_id == 0x5664||
        s_ctrl->sensordata->slave_info.sensor_id == 0x0616||
        s_ctrl->sensordata->slave_info.sensor_id == 0x0615||
        s_ctrl->sensordata->slave_info.sensor_id == 0x8054||
        s_ctrl->sensordata->slave_info.sensor_id == 0x88  ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x2b  ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x2b04||
        s_ctrl->sensordata->slave_info.sensor_id == 0x02  ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x686 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x789 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x841 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x766 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x766E||
        s_ctrl->sensordata->slave_info.sensor_id == 0x766F||
        s_ctrl->sensordata->slave_info.sensor_id == 0x682 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x890 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x355 ||
        s_ctrl->sensordata->slave_info.sensor_id == 0x3109){
            sensor_setting.reg_setting = sensor_settings.streamoff.reg_setting;
            sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
            sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
            sensor_setting.size = sensor_settings.streamoff.size;
            sensor_setting.delay = sensor_settings.streamoff.delay;
            rc = camera_io_dev_write(&(s_ctrl->io_master_info),&sensor_setting);
            if(rc < 0) {
                    /* If the I2C reg write failed for the first section reg, send
                    the result instead of keeping writing the next section of reg. */
                    CAM_ERR(CAM_SENSOR, "FTM Failed to stream off setting,rc=%d.",rc);
            } else {
                    CAM_ERR(CAM_SENSOR, "FTM successfully to stream off");
            }
    }
    rc = cam_sensor_power_down(s_ctrl);
    CAM_ERR(CAM_SENSOR, "FTM power down rc=%d",rc);
    return rc;
}

int cam_ftm_power_up(struct cam_sensor_ctrl_t *s_ctrl) {
    int rc = 0;
    
    rc = cam_sensor_power_up(s_ctrl);
    CAM_ERR(CAM_SENSOR, "FTM power up sensor id 0x%x,result %d",s_ctrl->sensordata->slave_info.sensor_id,rc);
    if(rc < 0) {
            CAM_ERR(CAM_SENSOR, "FTM power up faild!");
            return rc;
    }
    is_ftm_current_test =true;
    if (s_ctrl->sensordata->slave_info.sensor_id == 0x586) {
            CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
            sensor_setting.reg_setting = sensor_settings.imx586_setting0.reg_setting;
            sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
            sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
            sensor_setting.size = sensor_settings.imx586_setting0.size;
            sensor_setting.delay = sensor_settings.imx586_setting0.delay;
            rc = camera_io_dev_write(&(s_ctrl->io_master_info),&sensor_setting);
            if(rc < 0) {
                /* If the I2C reg write failed for the first section reg, send
                the result instead of keeping writing the next section of reg. */
                CAM_ERR(CAM_SENSOR, "FTM Failed to write sensor setting 1/2");
                goto power_down;;
            } else {
                CAM_ERR(CAM_SENSOR, "FTM successfully to write sensor setting 1/2");
            }
            sensor_setting.reg_setting = sensor_settings.imx586_setting1.reg_setting;
            sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
            sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
            sensor_setting.size = sensor_settings.imx586_setting1.size;
            sensor_setting.delay = sensor_settings.imx586_setting1.delay;
            rc = camera_io_dev_write(&(s_ctrl->io_master_info),&sensor_setting);
            if(rc < 0) {
                CAM_ERR(CAM_SENSOR, "FTM Failed to write sensor setting 2/2");
                goto power_down;;
            } else {
                CAM_ERR(CAM_SENSOR, "FTM successfully to write sensor setting 2/2");
            }
    } else {
            if (s_ctrl->sensordata->slave_info.sensor_id == 0x30d5) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.s5k3m5_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.size = sensor_settings.s5k3m5_setting.size;
                sensor_setting.delay = sensor_settings.s5k3m5_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x5035) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.gc5035_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.gc5035_setting.size;
                sensor_setting.delay = sensor_settings.gc5035_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x471) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx471_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx471_setting.size;
                sensor_setting.delay = sensor_settings.imx471_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x481) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx481_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx481_setting.size;
                sensor_setting.delay = sensor_settings.imx481_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x689) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx689_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx689_setting.size;
                sensor_setting.delay = sensor_settings.imx689_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x2375) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.gc2375_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.gc2375_setting.size;
                sensor_setting.delay = sensor_settings.gc2375_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x4608) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.hi846_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.size = sensor_settings.hi846_setting.size;
                sensor_setting.delay = sensor_settings.hi846_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if ((s_ctrl->sensordata->slave_info.sensor_id == 0x5664) ||
                (s_ctrl->sensordata->slave_info.sensor_id == 0x6b64) ||
                (s_ctrl->sensordata->slave_info.sensor_id == 0x5a64)) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.ov64b_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.ov64b_setting.size;
                sensor_setting.delay = sensor_settings.ov64b_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x0615) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx615_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx615_setting.size;
                sensor_setting.delay = sensor_settings.imx615_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x0616) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx616_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx616_setting.size;
                sensor_setting.delay = sensor_settings.imx616_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x8054) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.gc8054_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.gc8054_setting.size;
                sensor_setting.delay = sensor_settings.gc8054_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x88) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx616_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx616_setting.size;
                sensor_setting.delay = sensor_settings.imx616_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x2b ||
                       s_ctrl->sensordata->slave_info.sensor_id == 0x2b04) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.ov02b10_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.ov02b10_setting.size;
                sensor_setting.delay = sensor_settings.ov02b10_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x02e0 ||
                s_ctrl->sensordata->slave_info.sensor_id == 0xe0) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.gc02m1b_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.gc02m1b_setting.size;
                sensor_setting.delay = sensor_settings.gc02m1b_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x686) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx686_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx686_setting.size;
                sensor_setting.delay = sensor_settings.imx686_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x789) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx789_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx789_setting.size;
                sensor_setting.delay = sensor_settings.imx789_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else if (s_ctrl->sensordata->slave_info.sensor_id == 0x841) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.ov08a10_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.ov08a10_setting.size;
                sensor_setting.delay = sensor_settings.ov08a10_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x766 ||
                        s_ctrl->sensordata->slave_info.sensor_id == 0x766E ||
                        s_ctrl->sensordata->slave_info.sensor_id == 0x766F) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx766_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx766_setting.size;
                sensor_setting.delay = sensor_settings.imx766_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x682) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx682_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx682_setting.size;
                sensor_setting.delay = sensor_settings.imx682_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x5647 ||
                        s_ctrl->sensordata->slave_info.sensor_id == 0x5646) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.ov8d10_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.ov8d10_setting.size;
                sensor_setting.delay = sensor_settings.ov8d10_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x709) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx709_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx709_setting.size;
                sensor_setting.delay = sensor_settings.imx709_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x08D1) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.s5kgm1st_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.size = sensor_settings.s5kgm1st_setting.size;
                sensor_setting.delay = sensor_settings.s5kgm1st_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x890) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx890_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx890_setting.size;
                sensor_setting.delay = sensor_settings.imx890_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x355) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.imx355_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
                sensor_setting.size = sensor_settings.imx355_setting.size;
                sensor_setting.delay = sensor_settings.imx355_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            }else if (s_ctrl->sensordata->slave_info.sensor_id == 0x3109) {
                CAM_ERR(CAM_SENSOR, "FTM sensor setting 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                sensor_setting.reg_setting = sensor_settings.s5k3p9_setting.reg_setting;
                sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_WORD;
                sensor_setting.size = sensor_settings.s5k3p9_setting.size;
                sensor_setting.delay = sensor_settings.s5k3p9_setting.delay;
                rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
            } else {
                CAM_ERR(CAM_SENSOR, "FTM unknown sensor id 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
                rc = -1;
            }
            if (rc < 0) {
                CAM_ERR(CAM_SENSOR, "FTM Failed to write sensor setting");
                goto power_down;
            } else {
                CAM_ERR(CAM_SENSOR, "FTM successfully to write sensor setting");
            }
    }
    return rc;
power_down:
    CAM_ERR(CAM_SENSOR, "FTM wirte setting failed,do power down");
    cam_sensor_power_down(s_ctrl);
    return rc;
}

bool cam_ftm_if_do(void){

        CAM_DBG(CAM_SENSOR, "ftm state :%d",is_ftm_current_test);
        return is_ftm_current_test;
}

#if IS_ENABLED(CONFIG_PROJECT_INFO)
void cam_fill_module_info(struct cam_sensor_ctrl_t *s_ctrl) {

    enum COMPONENT_TYPE CameraID;
    uint32_t count = 0, i;
    uint32_t chipid=0,rc=0;

    if (s_ctrl->id == 0)
        CameraID = R_CAMERA;
    else if (s_ctrl->id == 1)
        CameraID = SECOND_R_CAMERA;
    else if (s_ctrl->id == 2)
        CameraID = F_CAMERA;
    else if (s_ctrl->id == 3)
        CameraID = THIRD_R_CAMERA;
    else if (s_ctrl->id == 4)
        CameraID = FORTH_R_CAMERA;
    else
        CameraID = -1;
    count = ARRAY_SIZE(match_tbl);
    for (i = 0; i < count; i++) {
        if (s_ctrl->sensordata->slave_info.sensor_id == match_tbl[i].sensor_id)
            break;
    }
    if(s_ctrl->sensordata->slave_info.sensor_id == 0x789) {
        rc = camera_io_dev_read(
                &(s_ctrl->io_master_info),
                0x0018,
                &chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
                CAMERA_SENSOR_I2C_TYPE_BYTE);
        if(rc){
                CAM_ERR(CAM_SENSOR, "read sensor vendor failed rc=%d",rc);
        } else {
                CAM_INFO(CAM_SENSOR, "read sensor vendor success rc=0x%x",chipid);
                if(chipid == 0) {
                        strcpy(match_tbl[14].sensor_name,"imx789");
                        chip_version_old = TRUE;
                }else if(chipid == 1){
                        strcpy(match_tbl[14].sensor_name,"imx789_0.91");
                        chip_version_old = TRUE;
                }else if(chipid == 2){
                        strcpy(match_tbl[14].sensor_name,"imx789_1.0");
                        chip_version_old = FALSE;
                }else if(chipid == 3){
                        strcpy(match_tbl[14].sensor_name,"imx789_1.1");
                        chip_version_old = FALSE;
                }else if(chipid > 3){
                        strcpy(match_tbl[14].sensor_name,"imx789_MP");
                        chip_version_old = FALSE;
                }
        }
    }
    else if(s_ctrl->sensordata->slave_info.sensor_id == 0x766) {
        rc = camera_io_dev_read(
                &(s_ctrl->io_master_info),
                0x0018,
                &chipid, CAMERA_SENSOR_I2C_TYPE_WORD,
                CAMERA_SENSOR_I2C_TYPE_BYTE);
        if(rc){
                CAM_ERR(CAM_SENSOR, "read sensor vendor failed rc=%d",rc);
        } else {
                CAM_INFO(CAM_SENSOR, "read sensor vendor success rc=0x%x",chipid);
                if(chipid == 0) {
                        strcpy(match_tbl[15].sensor_name,"imx766_0.9");
                }else if(chipid == 1){
                        strcpy(match_tbl[15].sensor_name,"imx766_0.91");
                }else if(chipid >= 9){
                        strcpy(match_tbl[15].sensor_name,"imx766_MP");
                }
        }
    }
    if (i >= count)
        CAM_ERR(CAM_SENSOR, "current sensor name is 0x%x",s_ctrl->sensordata->slave_info.sensor_id);
#if IS_ENABLED(CONFIG_PROJECT_INFO)
    else
        push_component_info(CameraID, match_tbl[i].sensor_name,match_tbl[i].vendor_name);
#endif

}
#else
void cam_fill_module_info(struct cam_sensor_ctrl_t *s_ctrl)
{
}
#endif

int32_t cam_sensor_update_id_info(struct cam_cmd_probe *probe_info,
    struct cam_sensor_ctrl_t *s_ctrl)
{
    int32_t rc = 0;

    s_ctrl->sensordata->id_info.sensor_slave_addr =
        probe_info->reserved;
    s_ctrl->sensordata->id_info.sensor_id_reg_addr =
        probe_info->reg_addr;
    s_ctrl->sensordata->id_info.sensor_id_mask =
        probe_info->data_mask;
    s_ctrl->sensordata->id_info.sensor_id =
        probe_info->expected_data;
    s_ctrl->sensordata->id_info.sensor_addr_type =
        probe_info->addr_type;
    s_ctrl->sensordata->id_info.sensor_data_type =
        probe_info->data_type;

    CAM_ERR(CAM_SENSOR,
        "vendor_slave_addr:  0x%x, vendor_id_Addr: 0x%x, vendorID: 0x%x, vendor_mask: 0x%x",
        s_ctrl->sensordata->id_info.sensor_slave_addr,
        s_ctrl->sensordata->id_info.sensor_id_reg_addr,
        s_ctrl->sensordata->id_info.sensor_id,
        s_ctrl->sensordata->id_info.sensor_id_mask);
    return rc;
}
uint32_t cam_override_chipid(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;
	uint32_t ee_vcmid = 0 ;
	uint32_t ee_moduleid = 0 ;
	uint32_t chipid = 0 ;

	struct cam_sensor_cci_client ee_cci_client ;
	struct cam_camera_slave_info *slave_info;

	const uint8_t IMX766_EEPROM_SID = (0xA2 >> 1);
	const uint8_t IMX766_EEPROM_VCMID_ADDR = 0x0A;
	const uint8_t IMX766_FIRST_SOURCE_VCMID = 0xE6;
	const uint8_t IMX766_SECOND_SOURCE_VCMID = 0xE9;
	const uint32_t IMX766_FIRST_SOURCE_CHIPID = 0x766F;
	const uint32_t IMX766_SECOND_SOURCE_CHIPID = 0x766E;
	const uint8_t OV08D10_EEPROM_SID = (0xA2 >> 1);
	const uint8_t OV08D10_EEPROM_MODULEID_ADDR = 0x00;
	const uint8_t OV08D10_FIRST_SOURCE_MODULEID = 0x05;
	const uint8_t OV08D10_SECOND_SOURCE_MODULEID = 0x01;
	const uint32_t OV08D10_FIRST_SOURCE_CHIPID = 0x5647;
	const uint32_t OV08D10_SECOND_SOURCE_CHIPID = 0x5646;

	slave_info = &(s_ctrl->sensordata->slave_info);

	if (!slave_info) {
		CAM_ERR(CAM_SENSOR, " failed: %pK",
			 slave_info);
		return -EINVAL;
	}

	if (slave_info->sensor_id == IMX766_FIRST_SOURCE_CHIPID || \
		  slave_info->sensor_id == IMX766_SECOND_SOURCE_CHIPID) {
		memcpy(&ee_cci_client, s_ctrl->io_master_info.cci_client,
						sizeof(struct cam_sensor_cci_client));
		ee_cci_client.sid = IMX766_EEPROM_SID;
		rc = cam_cci_i2c_read(&ee_cci_client,
			 IMX766_EEPROM_VCMID_ADDR,
			 &ee_vcmid, CAMERA_SENSOR_I2C_TYPE_WORD,
			 CAMERA_SENSOR_I2C_TYPE_BYTE);

		CAM_ERR(CAM_SENSOR, "distinguish imx766 camera module, vcm id : 0x%x ",ee_vcmid);
		if (IMX766_FIRST_SOURCE_VCMID == ee_vcmid) {
			chipid = IMX766_FIRST_SOURCE_CHIPID;
		} else if (IMX766_SECOND_SOURCE_VCMID == ee_vcmid) {
			chipid = IMX766_SECOND_SOURCE_CHIPID;
		}else{
			chipid = IMX766_FIRST_SOURCE_CHIPID;
		}
	}
	else if (slave_info->sensor_id == OV08D10_FIRST_SOURCE_CHIPID || \
		slave_info->sensor_id == OV08D10_SECOND_SOURCE_CHIPID) {
		memcpy(&ee_cci_client, s_ctrl->io_master_info.cci_client,
				sizeof(struct cam_sensor_cci_client));
		ee_cci_client.sid = OV08D10_EEPROM_SID;
		rc = cam_cci_i2c_read(&ee_cci_client,
			OV08D10_EEPROM_MODULEID_ADDR,
			&ee_moduleid, CAMERA_SENSOR_I2C_TYPE_WORD,
			CAMERA_SENSOR_I2C_TYPE_BYTE);

		CAM_ERR(CAM_SENSOR, "distinguish ov08d10 camera module, moudle id : 0x%x , sensor id : 0x%x",ee_moduleid,slave_info->sensor_id);
		if (OV08D10_FIRST_SOURCE_MODULEID == ee_moduleid) {
			chipid = OV08D10_FIRST_SOURCE_CHIPID;
		} else if (OV08D10_SECOND_SOURCE_MODULEID == ee_moduleid) {
			chipid = OV08D10_SECOND_SOURCE_CHIPID;
		} else {
			chipid = OV08D10_FIRST_SOURCE_CHIPID;
		}
	}
	else{
		rc = camera_io_dev_read(
			&(s_ctrl->io_master_info),
			slave_info->sensor_id_reg_addr,
			&chipid,slave_info->addr_type,
			slave_info->data_type);
	}
	return chipid;

}

int cam_sensor_match_id_oem(struct cam_sensor_ctrl_t *s_ctrl,uint32_t chip_id)
{
    uint32_t vendor_id = 0;
    int rc=0;

    if(chip_id == 0x766 || chip_id == 0x766E || chip_id == 0x766F){
            rc=camera_io_dev_read(
                        &(s_ctrl->io_master_info),
                        s_ctrl->sensordata->id_info.sensor_id_reg_addr,
                        &vendor_id,s_ctrl->sensordata->id_info.sensor_addr_type,
                        CAMERA_SENSOR_I2C_TYPE_BYTE);
            CAM_ERR(CAM_SENSOR, "read vendor_id_addr=0x%x vendor_id: 0x%x expected vendor_id 0x%x: rc=%d",
                    s_ctrl->sensordata->id_info.sensor_id_reg_addr,vendor_id, s_ctrl->sensordata->id_info.sensor_id,rc);
            /*if vendor id <1 ,it is old module if vendor_id >=1,it is 0.91 or 1.0 module*/
            if(vendor_id<1){
                        if(s_ctrl->sensordata->id_info.sensor_id<2)
                                return 0;
                        else
                                return -1;
            }else{
                        if(s_ctrl->sensordata->id_info.sensor_id>=9)
                                return 0;
                        else
                                return -1;
            }
    }

    if(chip_id==0x789){
            rc=camera_io_dev_read(
                        &(s_ctrl->io_master_info),
                        s_ctrl->sensordata->id_info.sensor_id_reg_addr,
                        &vendor_id,s_ctrl->sensordata->id_info.sensor_addr_type,
                        CAMERA_SENSOR_I2C_TYPE_BYTE);
            CAM_ERR(CAM_SENSOR, "read vendor_id_addr=0x%x vendor_id: 0x%x expected vendor_id 0x%x: rc=%d",
                    s_ctrl->sensordata->id_info.sensor_id_reg_addr,vendor_id, s_ctrl->sensordata->id_info.sensor_id,rc);
            /*if vendor id <2 ,it is old module if vendor_id >=9,it is 0.91 or 1.0 module*/
            if((vendor_id >> 4) == 0){
                        if(s_ctrl->sensordata->id_info.sensor_id == 0)
                                return 0;
                        else
                                return -1;
            }else if((vendor_id >> 4) == 1){
                        if(s_ctrl->sensordata->id_info.sensor_id == 1)
                                return 0;
                        else
                                return -1;
            }
            if(vendor_id == 0) {
                    chip_version_old = TRUE;
            }else if(vendor_id == 1){
                    chip_version_old = TRUE;
            }else if(vendor_id == 2){
                    chip_version_old = FALSE;
            }else if(vendor_id == 3){
                    chip_version_old = FALSE;
            }else if(vendor_id > 3){
                    chip_version_old = FALSE;
            }
    }
    return 0;
}
void cam_sensor_get_dt_data(struct cam_sensor_ctrl_t *s_ctrl)
{
        int32_t rc = 0;
        struct device_node *of_node = s_ctrl->of_node;

        rc = of_property_read_u32(of_node, "is-support-laser",&s_ctrl->is_support_laser);
        if ( rc < 0) {
                CAM_DBG(CAM_SENSOR, "Invalid sensor params");
                s_ctrl->is_support_laser = 0;
        }

        rc = of_property_read_u32(of_node, "is-read-eeprom",&s_ctrl->is_read_eeprom);
        if ( rc < 0) {
                CAM_DBG(CAM_SENSOR, "Invalid sensor params");
                s_ctrl->is_read_eeprom = 0;
        }
}

struct cam_sensor_dpc_reg_setting_array {
	struct cam_sensor_i2c_reg_array reg_setting[25];
	unsigned short size;
	enum camera_sensor_i2c_type addr_type;
	enum camera_sensor_i2c_type data_type;
	unsigned short delay;
};

struct cam_sensor_dpc_reg_setting_array gc5035OTPWrite_setting[7] = {
#include "CAM_GC5035_SPC_SENSOR_SETTINGS.h"
};

uint32_t totalDpcNum = 0;
uint32_t totalDpcFlag = 0;
uint32_t gc5035_chipversion_buffer[26]={0};

int sensor_gc5035_get_dpc_data(struct cam_sensor_ctrl_t * s_ctrl)
{
	int rc = 0;
	uint32_t gc5035_dpcinfo[3] = {0};
	uint32_t i;
	uint32_t dpcinfoOffet = 0xcd;
	uint32_t chipPage8Offet = 0xd0;
	uint32_t chipPage9Offet = 0xc0;

	struct cam_sensor_i2c_reg_setting sensor_setting;
	/*write otp read init settings*/
	sensor_setting.reg_setting = gc5035OTPWrite_setting[0].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[0].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[0].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[0].size;
	sensor_setting.delay = gc5035OTPWrite_setting[0].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	/*write dpc page0 setting*/
	sensor_setting.reg_setting = gc5035OTPWrite_setting[1].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[1].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[1].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[1].size;
	sensor_setting.delay = gc5035OTPWrite_setting[1].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	/*read dpc data*/
	for (i = 0; i < 3; i++) {
	    rc = camera_io_dev_read(
	         &(s_ctrl->io_master_info),
	         dpcinfoOffet + i,
	         &gc5035_dpcinfo[i], CAMERA_SENSOR_I2C_TYPE_BYTE,
	         CAMERA_SENSOR_I2C_TYPE_BYTE);
	    if (rc < 0) {
	        CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to read dpc info sensor setting");
	        break;
	    }
	}

	if (rc < 0)
	   return rc;
	/*close read data*/
	sensor_setting.reg_setting = gc5035OTPWrite_setting[2].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[2].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[2].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[2].size;
	sensor_setting.delay = gc5035OTPWrite_setting[2].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}

	for (i = 0; i < 19; i++) {
	    CAM_INFO(CAM_SENSOR, "gc5035SpcWrite_setting gc5035_dpcinfo[0] = %x",gc5035_dpcinfo[i]);
	}
	if (gc5035_dpcinfo[0] == 1) {
	    totalDpcFlag = 1;
	    totalDpcNum = gc5035_dpcinfo[1] + gc5035_dpcinfo[2] ;
	    CAM_INFO(CAM_SENSOR, "gc5035SpcWrite_setting gc5035_dpcinfo[1] = %d",gc5035_dpcinfo[1]);
	    CAM_INFO(CAM_SENSOR, "gc5035SpcWrite_setting gc5035_dpcinfo[2] = %d",gc5035_dpcinfo[2]);
	    CAM_INFO(CAM_SENSOR, "gc5035SpcWrite_setting totalDpcNum = %d",totalDpcNum);

	}
	//write for update reg for page 8
	sensor_setting.reg_setting = gc5035OTPWrite_setting[5].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[5].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[5].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[5].size;
	sensor_setting.delay = gc5035OTPWrite_setting[5].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	for (i = 0; i < 0x10; i++) {
	    rc = camera_io_dev_read(
	         &(s_ctrl->io_master_info),
	         chipPage8Offet + i,
	         &gc5035_chipversion_buffer[i], CAMERA_SENSOR_I2C_TYPE_BYTE,
	         CAMERA_SENSOR_I2C_TYPE_BYTE);
	    if (rc < 0) {
	        CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to read dpc info sensor setting");
	        break;
	    }
	}
	/*close read data*/
	sensor_setting.reg_setting = gc5035OTPWrite_setting[2].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[2].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[2].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[2].size;
	sensor_setting.delay = gc5035OTPWrite_setting[2].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	//write for update reg for page 9
	sensor_setting.reg_setting = gc5035OTPWrite_setting[6].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[6].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[6].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[6].size;
	sensor_setting.delay = gc5035OTPWrite_setting[6].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	for (i = 0x00; i < 0x0a; i++) {
	    rc = camera_io_dev_read(
	          &(s_ctrl->io_master_info),
	          chipPage9Offet + i,
	          &gc5035_chipversion_buffer[0x10+i], CAMERA_SENSOR_I2C_TYPE_BYTE,
	          CAMERA_SENSOR_I2C_TYPE_BYTE);
	    if (rc < 0) {
	        CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to read dpc info sensor setting");
	        break;
	    }
	}
	/*close read data*/
	sensor_setting.reg_setting = gc5035OTPWrite_setting[2].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[2].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[2].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[2].size;
	sensor_setting.delay = gc5035OTPWrite_setting[2].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	return rc;

}

int sensor_gc5035_write_dpc_data(struct cam_sensor_ctrl_t * s_ctrl)
{
    int rc = 0;
    struct cam_sensor_i2c_reg_array gc5035SpcTotalNum_setting[2];
    struct cam_sensor_i2c_reg_setting sensor_setting;
    //for test
    struct cam_sensor_i2c_reg_array gc5035SRAM_setting;
    uint32_t temp_val[4];
    int j,i;

    if (totalDpcFlag == 0) {
        return 0;
    }
	sensor_setting.reg_setting = gc5035OTPWrite_setting[3].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[3].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[3].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[3].size;
	sensor_setting.delay = gc5035OTPWrite_setting[3].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	gc5035SpcTotalNum_setting[0].reg_addr = 0x01;
	gc5035SpcTotalNum_setting[0].reg_data = (totalDpcNum >> 8) & 0x07;
	gc5035SpcTotalNum_setting[0].delay = gc5035SpcTotalNum_setting[0].data_mask = 0;

	gc5035SpcTotalNum_setting[1].reg_addr = 0x02;
	gc5035SpcTotalNum_setting[1].reg_data = totalDpcNum & 0xff;
	gc5035SpcTotalNum_setting[1].delay = gc5035SpcTotalNum_setting[1].data_mask = 0;

	sensor_setting.reg_setting = gc5035SpcTotalNum_setting;
	sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.size = 2;
	sensor_setting.delay = 0;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}

	sensor_setting.reg_setting = gc5035OTPWrite_setting[4].reg_setting;
	sensor_setting.addr_type = gc5035OTPWrite_setting[4].addr_type;
	sensor_setting.data_type = gc5035OTPWrite_setting[4].data_type;
	sensor_setting.size = gc5035OTPWrite_setting[4].size;
	sensor_setting.delay = gc5035OTPWrite_setting[4].delay;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
   gc5035SpcTotalNum_setting[0].reg_addr = 0xfe;
	gc5035SpcTotalNum_setting[0].reg_data = 0x02;
	gc5035SpcTotalNum_setting[0].delay = gc5035SpcTotalNum_setting[0].data_mask = 0;

	gc5035SpcTotalNum_setting[1].reg_addr = 0xbe;
	gc5035SpcTotalNum_setting[1].reg_data = 0x00;
	gc5035SpcTotalNum_setting[1].delay = gc5035SpcTotalNum_setting[1].data_mask = 0;
	sensor_setting.reg_setting = gc5035SpcTotalNum_setting;
	sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.size = 2;
	sensor_setting.delay = 0;
	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	for (i=0; i<totalDpcNum*4; i++) {
	gc5035SRAM_setting.reg_addr = 0xaa;
	gc5035SRAM_setting.reg_data = i;
	gc5035SRAM_setting.delay = gc5035SRAM_setting.data_mask = 0;
	sensor_setting.reg_setting = &gc5035SRAM_setting;
	sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.size = 1;
	sensor_setting.delay = 0;
	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);
	for (j=0; j<4; j++) {
	    rc = camera_io_dev_read(
	         &(s_ctrl->io_master_info),
	         0xac,
	         &temp_val[j], CAMERA_SENSOR_I2C_TYPE_BYTE,
	         CAMERA_SENSOR_I2C_TYPE_BYTE);
	    if (rc < 0) {
	       CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to read dpc info sensor setting");
	       break;
	    }
	}
	 CAM_INFO(CAM_SENSOR,"GC5035_OTP_GC val0 = 0x%x , val1 = 0x%x , val2 = 0x%x,val3 = 0x%x \n",
	 temp_val[0],temp_val[1],temp_val[2],temp_val[3]);
	 CAM_INFO(CAM_SENSOR,"GC5035_OTP_GC x = %d , y = %d ,type = %d \n",
	        ((temp_val[1]&0x0f)<<8) + temp_val[0],((temp_val[2]&0x7f)<<4) + ((temp_val[1]&0xf0)>>4),(((temp_val[3]&0x01)<<1)+((temp_val[2]&0x80)>>7)));
	}

	gc5035SpcTotalNum_setting[0].reg_addr = 0xbe;
	gc5035SpcTotalNum_setting[0].reg_data = 0x01;
	gc5035SpcTotalNum_setting[0].delay = gc5035SpcTotalNum_setting[0].data_mask = 0;

	gc5035SpcTotalNum_setting[1].reg_addr = 0xfe;
	gc5035SpcTotalNum_setting[1].reg_data = 0x00;
	gc5035SpcTotalNum_setting[1].delay = gc5035SpcTotalNum_setting[1].data_mask = 0;

	sensor_setting.reg_setting = gc5035SpcTotalNum_setting;
	sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.size = 2;
	sensor_setting.delay = 0;
	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	   CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	   return rc;
	}
	return rc;
}

int sensor_gc5035_update_reg(struct cam_sensor_ctrl_t * s_ctrl)
{
	int rc = -1;
	uint8_t flag_chipv = 0;
	int i = 0;
	uint8_t VALID_FLAG = 0x01;
	uint8_t CHIPV_FLAG_OFFSET = 0x0;
	uint8_t CHIPV_OFFSET = 0x01;
	uint8_t reg_setting_size = 0;
	struct cam_sensor_i2c_reg_array gc5035_update_reg_setting[20];
	struct cam_sensor_i2c_reg_setting sensor_setting;
	CAM_DBG(CAM_SENSOR,"Enter");

	flag_chipv = gc5035_chipversion_buffer[CHIPV_FLAG_OFFSET];
	CAM_DBG(CAM_SENSOR,"gc5035 otp chipv flag_chipv: 0x%x", flag_chipv);
	if (VALID_FLAG != (flag_chipv & 0x03)) {
	    CAM_ERR(CAM_SENSOR,"gc5035 otp chip regs data is Empty/Invalid!");
	    return rc;
	}

	for (i = 0; i < 5; i++) {
	    if (VALID_FLAG == ((gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i] >> 3) & 0x01)) {
	        gc5035_update_reg_setting[reg_setting_size].reg_addr = 0xfe;
	        gc5035_update_reg_setting[reg_setting_size].reg_data = gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i] & 0x07;
	        gc5035_update_reg_setting[reg_setting_size].delay = gc5035_update_reg_setting[reg_setting_size].data_mask = 0;
	        reg_setting_size++;
	        gc5035_update_reg_setting[reg_setting_size].reg_addr = gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 1];
	        gc5035_update_reg_setting[reg_setting_size].reg_data = gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 2];
	        gc5035_update_reg_setting[reg_setting_size].delay = gc5035_update_reg_setting[reg_setting_size].data_mask = 0;
	        reg_setting_size++;

	        CAM_DBG(CAM_SENSOR,"gc5035 otp chipv : 0xfe=0x%x, addr[%d]=0x%x, value[%d]=0x%x", gc5035_chipversion_buffer[CHIPV_OFFSET +  5 * i] & 0x07,i*2,
	                gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 1],i*2,gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 2]);
	    }
	    if (VALID_FLAG == ((gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i] >> 7) & 0x01)) {
	        gc5035_update_reg_setting[reg_setting_size].reg_addr = 0xfe;
	        gc5035_update_reg_setting[reg_setting_size].reg_data = (gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i] & 0x70) >> 4;
	        gc5035_update_reg_setting[reg_setting_size].delay = gc5035_update_reg_setting[reg_setting_size].data_mask = 0;
	        reg_setting_size++;
	        gc5035_update_reg_setting[reg_setting_size].reg_addr = gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 3];
	        gc5035_update_reg_setting[reg_setting_size].reg_data = gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 4];
	        gc5035_update_reg_setting[reg_setting_size].delay = gc5035_update_reg_setting[reg_setting_size].data_mask = 0;
	        reg_setting_size++;

	        CAM_DBG(CAM_SENSOR,"gc5035 otp chipv : 0xfe=0x%x, addr[%d]=0x%x, value[%d]=0x%x", (gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i] & 0x70) >> 4,i*2+1,
	                gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 3],i*2+1,gc5035_chipversion_buffer[CHIPV_OFFSET + 5 * i + 4]);
	    }
	}
	sensor_setting.reg_setting = gc5035_update_reg_setting;
	sensor_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	sensor_setting.size = reg_setting_size;
	sensor_setting.delay = 0;

	rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_setting);

	if (rc < 0) {
	    CAM_ERR(CAM_SENSOR, "gc5035SpcWrite_setting Failed to write sensor setting");
	    return rc;
	}
	rc = 0;
	CAM_DBG(CAM_SENSOR,"Exit");
	return rc;

}
/******************** GC5035_OTP_EDIT_END*******************/

int oplus_sensor_sony_get_dpc_data(struct cam_sensor_ctrl_t *s_ctrl)
{
    int i = 0, j = 0;
    int rc = 0;
    int check_reg_val, dfct_data_h, dfct_data_l;
    int dfct_data = 0;
    int fd_dfct_num = 0, sg_dfct_num = 0;
    int retry_cnt = 5;
    int data_h = 0, data_v = 0;
    int fd_dfct_addr = FD_DFCT_ADDR;
    int sg_dfct_addr = SG_DFCT_ADDR;

    CAM_INFO(CAM_SENSOR, "oplus_sensor_sony_get_dpc_data enter");
    if (s_ctrl == NULL) {
        CAM_ERR(CAM_SENSOR, "Invalid Args");
        return -EINVAL;
    }

    memset(&sony_dfct_tbl, 0, sizeof(struct sony_dfct_tbl_t));

    for (i = 0; i < retry_cnt; i++) {
        check_reg_val = 0;
        rc = camera_io_dev_read(&(s_ctrl->io_master_info),
            FD_DFCT_NUM_ADDR, &check_reg_val,
            CAMERA_SENSOR_I2C_TYPE_WORD,
            CAMERA_SENSOR_I2C_TYPE_BYTE);

        if (0 == rc) {
            fd_dfct_num = check_reg_val & 0x07;
            if (fd_dfct_num > FD_DFCT_MAX_NUM)
                fd_dfct_num = FD_DFCT_MAX_NUM;
            break;
        }
    }

    for (i = 0; i < retry_cnt; i++) {
        check_reg_val = 0;
        rc = camera_io_dev_read(&(s_ctrl->io_master_info),
            SG_DFCT_NUM_ADDR, &check_reg_val,
            CAMERA_SENSOR_I2C_TYPE_WORD,
            CAMERA_SENSOR_I2C_TYPE_WORD);

        if (0 == rc) {
            sg_dfct_num = check_reg_val & 0x01FF;
            if (sg_dfct_num > SG_DFCT_MAX_NUM)
                sg_dfct_num = SG_DFCT_MAX_NUM;
            break;
        }
    }

    CAM_INFO(CAM_SENSOR, " fd_dfct_num = %d, sg_dfct_num = %d", fd_dfct_num, sg_dfct_num);
    sony_dfct_tbl.fd_dfct_num = fd_dfct_num;
    sony_dfct_tbl.sg_dfct_num = sg_dfct_num;

    if (fd_dfct_num > 0) {
        for (j = 0; j < fd_dfct_num; j++) {
            dfct_data = 0;
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_h = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        fd_dfct_addr, &dfct_data_h,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_l = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        fd_dfct_addr+2, &dfct_data_l,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            CAM_DBG(CAM_SENSOR, " dfct_data_h = 0x%x, dfct_data_l = 0x%x", dfct_data_h, dfct_data_l);
            dfct_data = (dfct_data_h << 16) | dfct_data_l;
            data_h = 0;
            data_v = 0;
            data_h = (dfct_data & (H_DATA_MASK >> j%8)) >> (19 - j%8); //19 = 32 -13;
            data_v = (dfct_data & (V_DATA_MASK >> j%8)) >> (7 - j%8);  // 7 = 32 -13 -12;
            CAM_DBG(CAM_SENSOR, "j = %d, H = %d, V = %d", j, data_h, data_v);
            sony_dfct_tbl.fd_dfct_addr[j] = ((data_h & 0x1FFF) << V_ADDR_SHIFT) | (data_v & 0x0FFF);
            CAM_DBG(CAM_SENSOR, "fd_dfct_data[%d] = 0x%08x", j, sony_dfct_tbl.fd_dfct_addr[j]);
            fd_dfct_addr = fd_dfct_addr + 3 + ((j+1)%8 == 0);
        }
    }
    if (sg_dfct_num > 0) {
        for (j = 0; j < sg_dfct_num; j++) {
            dfct_data = 0;
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_h = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        sg_dfct_addr, &dfct_data_h,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            for (i = 0; i < retry_cnt; i++) {
                dfct_data_l = 0;
                rc = camera_io_dev_read(&(s_ctrl->io_master_info),
                        sg_dfct_addr+2, &dfct_data_l,
                        CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_WORD);
                if (0 == rc) {
                    break;
                }
            }
            CAM_DBG(CAM_SENSOR, " dfct_data_h = 0x%x, dfct_data_l = 0x%x", dfct_data_h, dfct_data_l);
            dfct_data = (dfct_data_h << 16) | dfct_data_l;
            data_h = 0;
            data_v = 0;
            data_h = (dfct_data & (H_DATA_MASK >> j%8)) >> (19 - j%8); //19 = 32 -13;
            data_v = (dfct_data & (V_DATA_MASK >> j%8)) >> (7 - j%8);  // 7 = 32 -13 -12;
            CAM_DBG(CAM_SENSOR, "j = %d, H = %d, V = %d", j, data_h, data_v);
            sony_dfct_tbl.sg_dfct_addr[j] = ((data_h & 0x1FFF) << V_ADDR_SHIFT) | (data_v & 0x0FFF);
            CAM_DBG(CAM_SENSOR, "sg_dfct_data[%d] = 0x%08x", j, sony_dfct_tbl.sg_dfct_addr[j]);
            sg_dfct_addr = sg_dfct_addr + 3 + ((j+1)%8 == 0);
        }
    }

    CAM_INFO(CAM_SENSOR, "exit");
    return rc;
}

int oplus_cam_sensor_update_setting(struct cam_sensor_ctrl_t *s_ctrl)
{
        int rc=0;
        struct cam_sensor_cci_client ee_cci_client;
        uint32_t sensor_version = 0;
        uint32_t qsc_address = 0;
        struct cam_camera_slave_info *slave_info;
        slave_info = &(s_ctrl->sensordata->slave_info);

        if (slave_info->sensor_id == 0x0766 ||
                slave_info->sensor_id == 0x0766E ||
                slave_info->sensor_id == 0x0766F) {
                memcpy(&ee_cci_client, s_ctrl->io_master_info.cci_client,
                        sizeof(struct cam_sensor_cci_client));
                if(s_ctrl->is_read_eeprom == 1) {
                        ee_cci_client.sid = 0xA2 >> 1;
                        qsc_address = 0x2BD0;
                }else if(s_ctrl->is_read_eeprom == 2){
                        ee_cci_client.sid = 0xA0 >> 1;
                        qsc_address = 0x2A32;
                }else if(s_ctrl->is_read_eeprom == 0){
                        CAM_INFO(CAM_SENSOR, "no need to update qsc tool");
                        return rc;
                }
                rc = cam_cci_i2c_read(&ee_cci_client,
                        qsc_address,
                        &sensor_version, CAMERA_SENSOR_I2C_TYPE_WORD,
                        CAMERA_SENSOR_I2C_TYPE_BYTE);
                CAM_INFO(CAM_SENSOR, "QSC tool version is %x",
                        sensor_version);
                if (sensor_version == 0x03) {
                        struct cam_sensor_i2c_reg_array qsc_tool = {
                                .reg_addr = 0x86A9,
                                .reg_data = 0x4E,
                                .delay = 0x00,
                                .data_mask = 0x00,
                        };
                        struct cam_sensor_i2c_reg_setting qsc_tool_write = {
                                .reg_setting = &qsc_tool,
                                .size = 1,
                                .addr_type = CAMERA_SENSOR_I2C_TYPE_WORD,
                                .data_type = CAMERA_SENSOR_I2C_TYPE_BYTE,
                                .delay = 0x00,
                        };
                        rc = camera_io_dev_write(&(s_ctrl->io_master_info), &qsc_tool_write);
                        CAM_INFO(CAM_SENSOR, "update the qsc tool version %d", rc);
                }
        }
        return rc;
}

int sensor_start_thread(void *arg)
{
	struct cam_sensor_ctrl_t *s_ctrl = (struct cam_sensor_ctrl_t *)arg;
	struct cam_sensor_i2c_reg_setting sensor_init_setting;
	int rc = 0;

	CAM_INFO(CAM_SENSOR, "sensor power start thread run...");

	if (!s_ctrl) {
		CAM_ERR(CAM_SENSOR, "s_ctrl is NULL");
		return -1;
	}
	mutex_lock(&(s_ctrl->cam_sensor_mutex));

	/*power up for sensor*/
	mutex_lock(&(s_ctrl->sensor_power_state_mutex));
	if (CAM_SENSOR_POWER_OFF == s_ctrl->sensor_power_state) {
		rc = cam_sensor_power_up(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "sensor power up faild!");
		} else {
			s_ctrl->sensor_power_state = CAM_SENSOR_POWER_ON;
		}
	} else {
		CAM_INFO(CAM_SENSOR, "sensor have power up!");
	}
	mutex_unlock(&(s_ctrl->sensor_power_state_mutex));

	/*write initsetting for sensor*/
	if (0 == rc) {
		mutex_lock(&(s_ctrl->sensor_initsetting_mutex));
		if (CAM_SENSOR_SETTING_WRITE_INVALID == s_ctrl->sensor_initsetting_state) {
			CAM_INFO(CAM_SENSOR, "start write sensor_id 0x%x init setting, sensor_slave_addr 0x%x",
				s_ctrl->sensordata->slave_info.sensor_id,
				s_ctrl->sensordata->slave_info.sensor_slave_addr);

			memset(&sensor_init_setting, 0, sizeof(struct cam_sensor_i2c_reg_setting)); //init
			if (0x5664 == s_ctrl->sensordata->slave_info.sensor_id) {
				sensor_init_setting.reg_setting = sensor_init_settings.ov64b_init_setting1.reg_setting;
				sensor_init_setting.addr_type = sensor_init_settings.ov64b_init_setting1.addr_type;
				sensor_init_setting.data_type = sensor_init_settings.ov64b_init_setting1.data_type;
				sensor_init_setting.size = sensor_init_settings.ov64b_init_setting1.size;
				sensor_init_setting.delay = sensor_init_settings.ov64b_init_setting1.delay;
			}else if(0x709 == s_ctrl->sensordata->slave_info.sensor_id) {
				sensor_init_setting.reg_setting = sensor_init_settings.imx709_init_setting1.reg_setting;
				sensor_init_setting.addr_type = sensor_init_settings.imx709_init_setting1.addr_type;
				sensor_init_setting.data_type = sensor_init_settings.imx709_init_setting1.data_type;
				sensor_init_setting.size = sensor_init_settings.imx709_init_setting1.size;
				sensor_init_setting.delay = sensor_init_settings.imx709_init_setting1.delay;
			}else if(0x2b == s_ctrl->sensordata->slave_info.sensor_id) {
				sensor_init_setting.reg_setting = sensor_init_settings.ov02b_init_setting1.reg_setting;
				sensor_init_setting.addr_type = sensor_init_settings.ov02b_init_setting1.addr_type;
				sensor_init_setting.data_type = sensor_init_settings.ov02b_init_setting1.data_type;
				sensor_init_setting.size = sensor_init_settings.ov02b_init_setting1.size;
				sensor_init_setting.delay = sensor_init_settings.ov02b_init_setting1.delay;
			}else if(0x0890 == s_ctrl->sensordata->slave_info.sensor_id) {
				sensor_init_setting.reg_setting = sensor_init_settings.imx890_init_setting1.reg_setting;
				sensor_init_setting.addr_type = sensor_init_settings.imx890_init_setting1.addr_type;
				sensor_init_setting.data_type = sensor_init_settings.imx890_init_setting1.data_type;
				sensor_init_setting.size = sensor_init_settings.imx890_init_setting1.size;
				sensor_init_setting.delay = sensor_init_settings.imx890_init_setting1.delay;
			}else if(0x0355 == s_ctrl->sensordata->slave_info.sensor_id) {
				sensor_init_setting.reg_setting = sensor_init_settings.imx355_init_setting1.reg_setting;
				sensor_init_setting.addr_type = sensor_init_settings.imx355_init_setting1.addr_type;
				sensor_init_setting.data_type = sensor_init_settings.imx355_init_setting1.data_type;
				sensor_init_setting.size = sensor_init_settings.imx355_init_setting1.size;
				sensor_init_setting.delay = sensor_init_settings.imx355_init_setting1.delay;
			}else {
				CAM_INFO(CAM_SENSOR, "sensor id %d init setting don't exist, skip the init process...",
					s_ctrl->sensordata->slave_info.sensor_id);
				goto exit;
			}

			rc = camera_io_dev_write(&(s_ctrl->io_master_info), &sensor_init_setting);
			if (rc < 0) {
				CAM_ERR(CAM_SENSOR, "write setting failed!");
			} else {
				CAM_INFO(CAM_SENSOR, "write setting success!");
				s_ctrl->sensor_initsetting_state = CAM_SENSOR_SETTING_WRITE_SUCCESS;
			}
		} else {
			CAM_INFO(CAM_SENSOR, "sensor setting alread wrote or do not need to write it !");
		}
exit:
		mutex_unlock(&(s_ctrl->sensor_initsetting_mutex));
	}
	CAM_INFO(CAM_SENSOR, "sensor power start thread end...");

	mutex_unlock(&(s_ctrl->cam_sensor_mutex));
	return rc;
}

int cam_sensor_start(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

	if (NULL == s_ctrl) {
		CAM_ERR(CAM_SENSOR, "s_ctrl is null ");
		return -1;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	mutex_lock(&(s_ctrl->sensor_power_state_mutex));
	if (CAM_SENSOR_POWER_OFF == s_ctrl->sensor_power_state) {
		s_ctrl->sensor_open_thread = kthread_run(sensor_start_thread, s_ctrl, s_ctrl->device_name);
		if (!s_ctrl->sensor_open_thread) {
			CAM_ERR(CAM_SENSOR, "create sensor start thread failed");
			rc = -1;
		} else {
			CAM_INFO(CAM_SENSOR, "create sensor start thread success");
		}
	} else {
		CAM_INFO(CAM_SENSOR, "sensor have power up");
	}
	mutex_unlock(&(s_ctrl->sensor_power_state_mutex));
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return rc;
}

int cam_sensor_stop(struct cam_sensor_ctrl_t *s_ctrl)
{
	int rc = 0;

    if (NULL == s_ctrl) {
		CAM_ERR(CAM_SENSOR, "s_ctrl is null ");
		return -1;
	}

	mutex_lock(&(s_ctrl->cam_sensor_mutex));
	mutex_lock(&(s_ctrl->sensor_power_state_mutex));
	if (CAM_SENSOR_POWER_ON == s_ctrl->sensor_power_state) {
		rc = cam_sensor_power_down(s_ctrl);
		if (rc < 0) {
			CAM_ERR(CAM_SENSOR, "sensor power down faild!");
			rc = -1;
		} else {
			CAM_INFO(CAM_SENSOR, "sensor power down success sensor id 0x%x", s_ctrl->sensordata->slave_info.sensor_id);
			s_ctrl->sensor_power_state = CAM_SENSOR_POWER_OFF;
			mutex_lock(&(s_ctrl->sensor_initsetting_mutex));
			s_ctrl->sensor_initsetting_state = CAM_SENSOR_SETTING_WRITE_INVALID;
			mutex_unlock(&(s_ctrl->sensor_initsetting_mutex));
		}
	} else {
		CAM_INFO(CAM_SENSOR, "camera sensor have power down");
		s_ctrl->sensor_power_state = CAM_SENSOR_POWER_OFF;
		mutex_lock(&(s_ctrl->sensor_initsetting_mutex));
		s_ctrl->sensor_initsetting_state = CAM_SENSOR_SETTING_WRITE_INVALID;
		mutex_unlock(&(s_ctrl->sensor_initsetting_mutex));
	}
	mutex_unlock(&(s_ctrl->sensor_power_state_mutex));
	mutex_unlock(&(s_ctrl->cam_sensor_mutex));

	return rc;
}

