using OmegaEngine;

namespace Game;

// The finish-line trigger: stops the clock, keeps the best time in the save
// data, and shows the result on its own banner text entity.
public sealed class FinishLine : Script
{
    public string Rider  = "Bike";
    public string Banner = "ResultText";   // world-space text near the line

    public override void OnTriggerEnter(Entity other)
    {
        if (Race.Finished || other.Id != Entity.Find(Rider).Id)
            return;

        Race.Finished = true;
        Audio.Play("assets/audio/finish.wav");

        float best = Save.GetFloat("besttime", float.MaxValue);
        bool record = Race.Time < best;
        if (record)
            Save.SetFloat("besttime", Race.Time);

        var banner = Entity.Find(Banner);
        if (banner.IsValid)
            banner.SetText(record
                ? $"{Race.Time:0.00}s  NEW RECORD!"
                : $"{Race.Time:0.00}s   (best {best:0.00}s)");

        Console.WriteLine($"[C#] finish: {Race.Time:0.00}s{(record ? " (record)" : "")}");
    }
}