#include <rclcpp/rclcpp.hpp>
#include "status_interfaces/msg/system_status.hpp"
#include <chrono>
#include <string>
#include <sys/utsname.h>
#include <fstream>
#include <sstream>

using namespace std::chrono_literals;
using SystemStatus = status_interfaces::msg::SystemStatus;

struct CpuStat
{
    unsigned long long user{0};
    unsigned long long nice{0};
    unsigned long long system{0};
    unsigned long long idle{0};
    unsigned long long iowait{0};
    unsigned long long irq{0};
    unsigned long long softirq{0};
    unsigned long long steal{0};
};

bool read_cpu_stat(CpuStat &out)
{
    std::ifstream f("/proc/stat");
    if (!f.is_open())
    {
        return false;
    }
    std::string line;
    std::getline(f, line);
    std::istringstream iss(line);
    std::string cpu_label;
    iss >> cpu_label
        >> out.user >> out.nice >> out.system >> out.idle
        >> out.iowait >> out.irq >> out.softirq >> out.steal;
    return true;
}

double calc_cpu_percent(const CpuStat &prev, const CpuStat &curr)
{
    auto prev_idle = prev.idle + prev.iowait;
    auto curr_idle = curr.idle + curr.iowait;

    auto prev_total = prev.user + prev.nice + prev.system + prev.idle + prev.iowait + prev.irq + prev.softirq + prev.steal;
    auto curr_total = curr.user + curr.nice + curr.system + curr.idle + curr.iowait + curr.irq + curr.softirq + curr.steal;

    auto delta_total = curr_total - prev_total;
    auto delta_idle = curr_idle - prev_idle;

    if (delta_total == 0)
    {
        return 0.0;
    }
    return 100.0 * (delta_total - delta_idle) / delta_total;
}

bool read_mem_info(double &total_mb, double &available_mb)
{
    std::ifstream f("/proc/meminfo");
    if (!f.is_open())
    {
        return false;
    }
    std::string line;
    long long mem_total_kb = 0, mem_avail_kb = 0;
    while (std::getline(f, line))
    {
        std::istringstream iss(line);
        std::string key;
        long long val;
        std::string unit;
        iss >> key >> val >> unit;
        if (key == "MemTotal:")
        {
            mem_total_kb = val;
        }
        if (key == "MemAvailable:")
        {
            mem_avail_kb = val;
        }
    }
    if (mem_total_kb <= 0)
    {
        return false;
    }
    total_mb = static_cast<double>(mem_total_kb) / 1024.0;
    available_mb = static_cast<double>(mem_avail_kb) / 1024.0;
    return true;
}

class StatusPublisher : public rclcpp::Node
{
public:
    StatusPublisher() : Node("status_publisher")
    {
        publisher_ = this->create_publisher<SystemStatus>("system_status", 10);
        read_cpu_stat(last_cpu_);

        timer_ = this->create_wall_timer(
            1000ms,
            [this]()
            { timer_callback(); });
        RCLCPP_INFO(this->get_logger(), "status_publisher 启动，读取真实系统资源");
    }

private:
    void timer_callback()
    {
        auto msg = SystemStatus();
        msg.stamp = this->get_clock()->now();

        struct utsname sys_info;
        uname(&sys_info);
        msg.host_name = std::string(sys_info.nodename);

        // CPU
        CpuStat now_cpu;
        read_cpu_stat(now_cpu);
        msg.cpu_percent = calc_cpu_percent(last_cpu_, now_cpu);
        last_cpu_ = now_cpu;

        // 内存
        double mem_total, mem_avail;
        if (read_mem_info(mem_total, mem_avail))
        {
            msg.memory_total = mem_total;
            msg.memory_available = mem_avail;
            msg.memory_percent = 100.0 * (mem_total - mem_avail) / mem_total;
        }
        else
        {
            msg.memory_percent = 0;
            msg.memory_total = 0;
            msg.memory_available = 0;
        }

        // 网络（模拟，想要真实网卡流量我再给你加上）
        msg.net_sent = 120.5;
        msg.net_recv = 230.8;

        publisher_->publish(msg);
        RCLCPP_INFO(this->get_logger(),
                    "发布 | CPU:%.1f%% | MEM:%.1f%% | total:%.0f MB | avail:%.0f MB",
                    msg.cpu_percent, msg.memory_percent, msg.memory_total, msg.memory_available);
    }

    rclcpp::Publisher<SystemStatus>::SharedPtr publisher_;
    rclcpp::TimerBase::SharedPtr timer_;
    CpuStat last_cpu_;
};

int main(int argc, char *argv[])
{
    rclcpp::init(argc, argv);
    auto node = std::make_shared<StatusPublisher>();
    rclcpp::spin(node);
    rclcpp::shutdown();
    return 0;
}