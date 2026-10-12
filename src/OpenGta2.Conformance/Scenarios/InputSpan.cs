namespace OpenGta2.Conformance.Scenarios;

/// <summary>Buttons held for an inclusive tick range. Overlapping spans are combined.</summary>
public sealed record InputSpan
{
    public required int FromTick { get; init; }

    public required int ToTick { get; init; }

    public IReadOnlyList<PlayerButton> Buttons { get; init; } = [];
}

/// <summary>Logical player controls (GTA 2 keyboard actions), independent of key bindings.</summary>
public enum PlayerButton
{
    Forward,
    Backward,
    Left,
    Right,
    Attack,
    Jump,
    EnterExit,
    PreviousWeapon,
    NextWeapon,
    Special,
}

[Flags]
public enum PlayerButtons : ushort
{
    None = 0,
    Forward = 1 << PlayerButton.Forward,
    Backward = 1 << PlayerButton.Backward,
    Left = 1 << PlayerButton.Left,
    Right = 1 << PlayerButton.Right,
    Attack = 1 << PlayerButton.Attack,
    Jump = 1 << PlayerButton.Jump,
    EnterExit = 1 << PlayerButton.EnterExit,
    PreviousWeapon = 1 << PlayerButton.PreviousWeapon,
    NextWeapon = 1 << PlayerButton.NextWeapon,
    Special = 1 << PlayerButton.Special,
}

public static class PlayerButtonExtensions
{
    public static PlayerButtons ToFlags(this IEnumerable<PlayerButton> buttons) =>
        buttons.Aggregate(PlayerButtons.None, (acc, b) => acc | (PlayerButtons)(1 << (int)b));

    public static bool IsHeld(this PlayerButtons buttons, PlayerButton button) =>
        (buttons & (PlayerButtons)(1 << (int)button)) != 0;
}
