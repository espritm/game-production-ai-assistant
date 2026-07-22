using System.Diagnostics;

namespace GPAA.Api;

public sealed class ParentProcessMonitorService : BackgroundService
{
    private static readonly TimeSpan PollInterval = TimeSpan.FromSeconds(2);

    private readonly ParentProcessMonitorOptions _options;
    private readonly IHostApplicationLifetime _applicationLifetime;
    private readonly ILogger<ParentProcessMonitorService> _logger;

    public ParentProcessMonitorService(
        ParentProcessMonitorOptions options,
        IHostApplicationLifetime applicationLifetime,
        ILogger<ParentProcessMonitorService> logger)
    {
        _options = options;
        _applicationLifetime = applicationLifetime;
        _logger = logger;
    }

    protected override async Task ExecuteAsync(CancellationToken stoppingToken)
    {
        if (_options.GpaaParentProcessId <= 0)
        {
            return;
        }

        _logger.LogInformation("Monitoring parent process {ParentProcessId}.", _options.GpaaParentProcessId);

        while (!stoppingToken.IsCancellationRequested)
        {
            if (!IsProcessRunning(_options.GpaaParentProcessId))
            {
                _logger.LogInformation("Parent process {ParentProcessId} exited. Stopping GPAA backend.", _options.GpaaParentProcessId);
                _applicationLifetime.StopApplication();
                return;
            }

            await Task.Delay(PollInterval, stoppingToken);
        }
    }

    private static bool IsProcessRunning(int processId)
    {
        try
        {
            using var process = Process.GetProcessById(processId);
            return !process.HasExited;
        }
        catch (ArgumentException)
        {
            return false;
        }
        catch (InvalidOperationException)
        {
            return false;
        }
    }
}
