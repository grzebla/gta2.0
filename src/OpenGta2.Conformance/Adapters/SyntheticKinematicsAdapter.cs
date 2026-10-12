using OpenGta2.Conformance.Scenarios;
using OpenGta2.Conformance.Simulation;
using OpenGta2.Conformance.Tracing;

namespace OpenGta2.Conformance.Adapters;

/// <summary>
/// SYNTHETIC model used to exercise the framework end to end (input replay, fixed step,
/// RNG, capture, comparison). Its rules and constants are invented and do NOT describe
/// GTA 2 or OpenGTA2 behaviour. Only scenarios with provenance "synthetic" may use it.
/// </summary>
public sealed class SyntheticKinematicsAdapter : ISimulationAdapter
{
    public const string AdapterId = "synthetic-kinematics";

    private const double Acceleration = 4.0;     // units / s^2
    private const double Braking = 8.0;
    private const double Drag = 1.5;              // units / s^2 when no pedal is held
    private const double MaxSpeed = 6.0;
    private const double TurnRateDegrees = 180.0; // degrees / s

    private double _x, _y, _headingDegrees, _speed;
    private int _wandererX, _wandererY;
    private DeterministicRandom? _random;

    public string Id => AdapterId;

    public string Version => "1";

    public void Initialize(ScenarioDefinition scenario, SimulationContext context)
    {
        if (scenario.Provenance != ScenarioProvenance.Synthetic)
        {
            throw new InvalidOperationException($"{AdapterId} only runs synthetic scenarios.");
        }

        _x = _y = _headingDegrees = _speed = 0;
        _wandererX = _wandererY = 0;
        _random = context.Random;
    }

    public void Step(in StepInput input)
    {
        var dt = input.DeltaSeconds;
        var b = input.Buttons;

        if (b.IsHeld(PlayerButton.Forward))
        {
            _speed = Math.Min(MaxSpeed, _speed + Acceleration * dt);
        }
        else if (b.IsHeld(PlayerButton.Backward))
        {
            _speed = Math.Max(0, _speed - Braking * dt);
        }
        else
        {
            _speed = Math.Max(0, _speed - Drag * dt);
        }

        var turn = (b.IsHeld(PlayerButton.Left) ? 1 : 0) - (b.IsHeld(PlayerButton.Right) ? 1 : 0);
        _headingDegrees = (_headingDegrees + turn * TurnRateDegrees * dt) % 360.0;
        if (_headingDegrees < 0)
        {
            _headingDegrees += 360.0;
        }

        var radians = _headingDegrees * Math.PI / 180.0;
        _x += Math.Cos(radians) * _speed * dt;
        _y += Math.Sin(radians) * _speed * dt;

        // Random walk on a grid: exercises seeded randomness without floating point.
        switch (_random!.NextInt(4))
        {
            case 0: _wandererX++; break;
            case 1: _wandererX--; break;
            case 2: _wandererY++; break;
            default: _wandererY--; break;
        }
    }

    public IReadOnlyList<EntityState> Capture() =>
    [
        EntityState.Create("player", "synthetic-body",
            ("x", _x), ("y", _y), ("headingDegrees", _headingDegrees), ("speed", _speed)),
        EntityState.Create("wanderer", "synthetic-walker",
            ("cellX", _wandererX), ("cellY", _wandererY)),
    ];

    public void Dispose()
    {
    }
}
