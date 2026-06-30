#pragma once

/**
 * File:    DeviceProfile.hpp
 * Author:  Trung Nguyen
 * GitHub:  https://github.com/trungnguyen278/PTalk-V2
 * Date:    30 Jun 2026
 *
 * Description:
 *  - Part of the PTalk-V2 project
 *  - Written and maintained by Trung Nguyen
 */

class AppController;

class DeviceProfile
{
public:
    /**
     * Setup toàn bộ hệ thống trước khi AppController::start()
     *
     * @param app  AppController singleton
     * @return true nếu setup OK
     */
    static bool setup(AppController &app);
};
