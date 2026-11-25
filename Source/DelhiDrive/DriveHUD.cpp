#include "DriveHUD.h"
#include "DrivePlayerController.h"
#include "RoadNetworkActor.h"
#include "EngineUtils.h"
#include "DriveVehicle.h"
#include "DriveGameMode.h"
#include "CarCatalog.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"

void ADriveHUD::DrawHUD()
{
	Super::DrawHUD();

	ADrivePlayerController* PC = Cast<ADrivePlayerController>(GetOwningPlayerController());
	ADriveVehicle* V = PC ? Cast<ADriveVehicle>(PC->GetPawn()) : nullptr;
	ADriveGameMode* GM = GetWorld() ? GetWorld()->GetAuthGameMode<ADriveGameMode>() : nullptr;
	if (!PC || !GM || !Canvas) return;

	if (!V)
	{
		// The controller may be active one frame before its pawn.
		UFont* F = GEngine->GetLargeFont();
		const float Sc = Canvas->SizeY / 600.f;
		const FString Lines[] = {
			TEXT("Preparing your car..."),

		};
		float Y = Canvas->SizeY * 0.35f;
		for (const FString& L : Lines)
		{
			float W = 0.f, H = 0.f;
			GetTextSize(L, W, H, F, Sc);
			DrawText(L, FLinearColor(1.f, 0.85f, 0.2f, 1.f), (Canvas->SizeX - W) * 0.5f, Y, F, Sc);
			Y += H * 1.6f;
		}
		return;
	}

	const FVector2D VS(Canvas->SizeX, Canvas->SizeY);
	const float U = VS.Y * 0.1f;
	const float K = VS.Y / 900.f;
	const float CX = VS.X * 0.5f;
	UFont* Font = GEngine->GetLargeFont();

	auto Label = [&](const FString& S, float X, float Y, float Scale, const FLinearColor& C)
	{
		float W = 0.f, H = 0.f;
		float TextScale = Scale * K;
		GetTextSize(S, W, H, Font, TextScale);
		if (W > VS.X * 0.94f)
		{
			TextScale *= VS.X * 0.94f / W;
			GetTextSize(S, W, H, Font, TextScale);
		}
		DrawText(S, C, X - W * 0.5f, Y - H * 0.5f, Font, TextScale);
	};
	auto Box = [&](const FVector2D& C, const FVector2D& Hf, const FLinearColor& Fill, const FLinearColor& Line)
	{
		DrawRect(Fill, C.X - Hf.X, C.Y - Hf.Y, Hf.X * 2.f, Hf.Y * 2.f);
		DrawLine(C.X - Hf.X, C.Y - Hf.Y, C.X + Hf.X, C.Y - Hf.Y, Line, 3.f);
		DrawLine(C.X - Hf.X, C.Y + Hf.Y, C.X + Hf.X, C.Y + Hf.Y, Line, 3.f);
		DrawLine(C.X - Hf.X, C.Y - Hf.Y, C.X - Hf.X, C.Y + Hf.Y, Line, 3.f);
		DrawLine(C.X + Hf.X, C.Y - Hf.Y, C.X + Hf.X, C.Y + Hf.Y, Line, 3.f);
	};

	// ================================================================== MENU
	if (GM->bInMenu)
	{
        if (PC->bShowingCredits)
        {
            DrawRect(FLinearColor(.02f,.035f,.045f,.97f),0,0,VS.X,VS.Y);
            Label(TEXT("MAP AND ART CREDITS"),CX,U*1.0f,.90f,FLinearColor::White);
            const TCHAR* Lines[] = {
                TEXT("Roads and building outlines: OpenStreetMap contributors"),
                TEXT("Open Database Licence 1.0 | openstreetmap.org/copyright"),
                TEXT("BMW 320i by rifdanzz  /  Mercedes-Benz G-Class by Lexyc16"),
                TEXT("Mercedes-Benz AMG GT by Yan Carvalho  /  Sketchfab"),
                TEXT("Creative Commons Attribution 4.0 | creativecommons.org/licenses/by/4.0"),
                TEXT("Car meshes were combined and resized. The AMG GT was also simplified."),
                TEXT("Surface scans and daylight photography: Poly Haven (CC0).")
            };
            float Y=U*2.f;
            for (const TCHAR* Line : Lines) { Label(Line,CX,Y,.47f,FLinearColor(.84f,.89f,.9f)); Y+=U*.81f; }
            Label(TEXT("Tap to return to the garage"),CX,U*8.9,.55f,FLinearColor::White);
            return;
        }
		const FCarSpec& C = DriveCatalog::Car(GM->CarIndex);
		const bool bCarLocked = !GM->IsCarUnlocked(GM->CarIndex);
		const bool bLocked = bCarLocked || !GM->IsLevelUnlocked();
		const bool bSuper = C.Class == ECarClass::Supercar;
		const FLinearColor Accent = bSuper ? FLinearColor(1.f, 0.35f, 0.1f) : FLinearColor(1.f, 0.8f, 0.25f);
		const FLinearColor Gray(0.75f, 0.75f, 0.75f, 1.f);

		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), 0.f, 0.f, VS.X, VS.Y);
		Label(TEXT("DELHI DRIVE"), CX, U * 0.6f, 1.6f, FLinearColor(1.f, 0.85f, 0.2f, 1.f));
		Label(C.Name, CX, U * 1.8f, 1.25f, bCarLocked ? FLinearColor(0.6f, 0.6f, 0.6f, 1.f) : FLinearColor::White);
		Label(FString::Printf(TEXT("%s | %s"),bSuper ? TEXT("SUPERCAR") : TEXT("LUXURY"),*V->GetVisualDescription()), CX, U * 2.35f, 0.45f, Accent);

		auto Bar = [&](const TCHAR* Name, float Frac, float Y)
		{
			Label(Name, CX - U * 3.4f, Y, 0.5f, Gray);
			const float BW = U * 4.8f, BH = U * 0.2f, BX = CX - U * 1.7f;
			DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.6f), BX, Y - BH * 0.5f, BW, BH);
			DrawRect(Accent, BX, Y - BH * 0.5f, BW * FMath::Clamp(Frac, 0.f, 1.f), BH);
		};
		Bar(TEXT("POWER"),    C.PowerScale,         U * 2.85f);
		Bar(TEXT("TOP SPEED"), C.TopSpeedKmh / 400.f, U * 3.25f);
		Bar(TEXT("BRAKING"),  C.BrakeScale / 1.3f,  U * 3.65f);
		Bar(TEXT("HANDLING"), C.SteerScale / 1.3f,  U * 4.05f);

		if (bCarLocked)
		{
			Label(FString::Printf(TEXT("CAR LOCKED - needs %d pts (you have %d)"), C.UnlockScore, GM->TotalScore), CX, U * 4.55f, 0.5f, FLinearColor(1.f, 0.3f, 0.3f, 1.f));
		}
		else
		{
			Label(FString::Printf(TEXT("Career score %d     Difficulty best %d"), GM->TotalScore, GM->BestScores.IsValidIndex((int32)GM->Difficulty) ? GM->BestScores[(int32)GM->Difficulty] : 0), CX, U * 4.55f, 0.5f, Gray);
		}

		const FDifficultySpec& D = GM->Diff();
		Label(D.Name, CX, U * 5.15f, 1.0f, FLinearColor::White);
		Label(D.Blurb, CX, U * 5.65f, 0.45f, Gray);
		Label(GM->IsTour() ? FString(TEXT("OPEN WORLD TOUR - YOUR OWN ROUTE")) : FString::Printf(TEXT("%02d / %02d   %s"), GM->SelectedChallenge() + 1, GM->ChallengeCount(), GM->Level().Name), CX, U * 6.35f, 0.85f, Accent);
		const int32 Stars = GM->ChallengeStars();
		const FString LevelInfo = GM->IsTour() ? FString(TEXT("No timer or fuel limit. Choose your car, city and traffic difficulty.")) : GM->IsLevelUnlocked()
			? FString::Printf(TEXT("%d/3 stars | %s | %d markers | %.0f km/h"), Stars, DriveLevels::ObjectiveName(GM->Level().Objective), GM->TotalCheckpoints, GM->Level().SpeedLimitKmh)
			: TEXT("LEVEL LOCKED - complete the previous level");
		Label(LevelInfo, CX, U * 7.0f, 0.5f, Gray);
		Label(GM->IsTour() ? TEXT("Use RECOVER if stuck. Tour never changes career scores or stars.") : GM->Level().Instruction, CX, U * 7.55f, .38f, Gray);
		Label(TEXT("OpenStreetMap roads | simulated scenery / hazards"), CX, U * 9.15f, 0.45f, Gray);
		Label(TEXT("(c) OpenStreetMap contributors | openstreetmap.org/copyright"), CX, U * 9.65f, 0.4f, Gray);

		TArray<FDriveButton> Menu;
		ADrivePlayerController::BuildMenuLayout(VS, Menu);
		static const TCHAR* const GfxNames[3] = { TEXT("GRAPHICS: LOW"), TEXT("GRAPHICS: MEDIUM"), TEXT("GRAPHICS: HIGH") };
		for (const FDriveButton& B : Menu)
		{
			if (GM->IsTour() && (B.Id == FName("LEVEL_PREV") || B.Id == FName("LEVEL_NEXT"))) continue;
			FLinearColor Fill(0.f, 0.f, 0.f, 0.5f);
			FString Text = B.Label;
			if (B.Id == FName("SESSION")) Text = GM->SessionName();
			if (B.Id == FName("GFX")) Text = GfxNames[FMath::Clamp(GM->Quality, 0, 2)];
			if (B.Id == FName("CITY")) Text = FString::Printf(TEXT("CITY: %s"), GM->CityName());
			if (B.Id == FName("START"))
			{
				Fill = bLocked ? FLinearColor(0.25f, 0.25f, 0.25f, 0.8f) : FLinearColor(0.05f, 0.5f, 0.12f, 0.85f);
				Text = bLocked ? TEXT("LOCKED") : TEXT("START");
			}
			Box(B.Center, B.Half, Fill, FLinearColor(1.f, 1.f, 1.f, 0.7f));
			Label(Text, B.Center.X, B.Center.Y, B.Id == FName("START") ? 1.0f : ((B.Id == FName("CITY") || B.Id == FName("SESSION")) ? 0.40f : 0.6f), FLinearColor::White);
		}
		return;
	}

	// ================================================================== DRIVING
	TArray<FDriveButton> Buttons;
	FVector2D WC;
	float WR = 0.f;
	ADrivePlayerController::BuildLayout(VS, V->IsAutomatic(), Buttons, WC, WR);

	for (const FDriveButton& B : Buttons)
	{
		FLinearColor Fill(0.f, 0.f, 0.f, 0.40f);
		if (B.Id == FName("GAS"))        Fill = FLinearColor(0.05f, 0.45f, 0.10f, 0.45f);
		else if (B.Id == FName("BRAKE")) Fill = FLinearColor(0.55f, 0.05f, 0.05f, 0.45f);
		if (PC->IsHeld(B.Id)) Fill.A = 0.80f;

		FString Text = B.Label;
		if (B.Id == FName("MODE")) Text = V->IsAutomatic() ? TEXT("AUTO") : TEXT("MANUAL");
		if (B.Id == FName("DR"))   Text = V->IsReverseSelected() ? TEXT("-> D") : TEXT("-> R");

		Box(B.Center, B.Half, Fill, FLinearColor(1.f, 1.f, 1.f, 0.7f));
		Label(Text, B.Center.X, B.Center.Y, 0.55f, FLinearColor::White);
	}

	// ---- steering wheel ----
	const FLinearColor WheelCol(0.07f, 0.07f, 0.07f, 0.90f);
	const int32 Segs = 40;
	for (int32 i = 0; i < Segs; ++i)
	{
		const float a0 = 2.f * PI * i / Segs;
		const float a1 = 2.f * PI * (i + 1) / Segs;
		DrawLine(WC.X + WR * FMath::Cos(a0), WC.Y + WR * FMath::Sin(a0),
		         WC.X + WR * FMath::Cos(a1), WC.Y + WR * FMath::Sin(a1), WheelCol, U * 0.18f);
	}
	const float Base[3] = { 180.f, 0.f, 90.f };
	for (int32 i = 0; i < 3; ++i)
	{
		const float a = FMath::DegreesToRadians(Base[i] + PC->WheelAngleDeg);
		DrawLine(WC.X, WC.Y, WC.X + WR * FMath::Cos(a), WC.Y + WR * FMath::Sin(a), WheelCol, U * 0.14f);
	}
	const float Top = FMath::DegreesToRadians(-90.f + PC->WheelAngleDeg);
	DrawLine(WC.X + WR * 0.82f * FMath::Cos(Top), WC.Y + WR * 0.82f * FMath::Sin(Top),
	         WC.X + WR * FMath::Cos(Top), WC.Y + WR * FMath::Sin(Top), FLinearColor(0.9f, 0.1f, 0.1f, 1.f), U * 0.2f);

	// ---- speed / gear / rpm ----
	Label(FString::FromInt(FMath::RoundToInt(V->GetSpeedKmh())), CX, VS.Y - U * 1.9f, 2.6f, FLinearColor::White);
	Label(TEXT("km/h"), CX, VS.Y - U * 0.95f, 0.5f, FLinearColor(0.8f, 0.8f, 0.8f, 1.f));
	Label(V->GetGearLabel(), CX + U * 2.4f, VS.Y - U * 1.9f, 1.6f, FLinearColor(1.f, 0.85f, 0.2f, 1.f));

	const float RW = U * 5.f, RH = U * 0.22f, RY = VS.Y - U * 0.45f;
	const float Rpm = V->GetRPMNormalized();
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), CX - RW * 0.5f, RY, RW, RH);
	DrawRect(FLinearColor::LerpUsingHSV(FLinearColor::Green, FLinearColor::Red, Rpm), CX - RW * 0.5f, RY, RW * Rpm, RH);

	// ---- fuel ----
	const float Fuel = V->FuelPercent;
	const float FW = U * 3.f;
	Label(TEXT("FUEL"), U * 0.5f + FW * 0.5f, U * 0.4f, 0.5f, FLinearColor::White);
	DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.5f), U * 0.5f, U * 0.7f, FW, U * 0.25f);
	DrawRect(Fuel > 15.f ? FLinearColor(1.f, 0.7f, 0.1f, 1.f) : FLinearColor::Red, U * 0.5f, U * 0.7f, FW * Fuel / 100.f, U * 0.25f);

	// ---- mission ----
	Label(GM->GetMissionStatus(), CX, U * 1.55f, 0.44f, FLinearColor::White);
	Label(GM->GetNavigationStatus(), CX, U * 2.1f, .48f, FLinearColor(1.f,.82f,.25f));
	Label(GM->GetObjectiveText(), CX, U * 3.45f, .43f, FLinearColor(.75f,1.f,.85f));

    // The map texture covers the same +/-1.2 km as the road and building extracts.
    // X points east and Y south, so north is already at the top in canvas coordinates.
    if (CachedCity != GM->CityIndex)
    {
        CachedCity = GM->CityIndex;
        const FString MapName = CachedCity == 0 ? TEXT("T_Map_delhi") : TEXT("T_Map_chandigarh");
        DistrictMap = LoadObject<UTexture2D>(nullptr, *FString::Printf(TEXT("/Game/Environment/%s.%s"), *MapName, *MapName));
    }
    const FVector2D Origin(U*.5,U*1.2), Size(U*3.4,U*2.5);
    const FVector2D WorldPos(V->GetActorLocation().X, V->GetActorLocation().Y);
    const FVector2D Span(66000.,66000.*Size.Y/Size.X);
    constexpr double Radius=120000.;
    const FVector2D Corner(
        FMath::Clamp(WorldPos.X-Span.X*.5,-Radius,Radius-Span.X),
        FMath::Clamp(WorldPos.Y-Span.Y*.5,-Radius,Radius-Span.Y));
    auto ToMap = [&](const FVector2D& P) { return Origin + (P-Corner)*(Size.X/Span.X); };
    auto Visible = [&](const FVector2D& P) {
        return P.X>=Origin.X && P.Y>=Origin.Y && P.X<=Origin.X+Size.X && P.Y<=Origin.Y+Size.Y;
    };
    DrawRect(FLinearColor(.035f,.07f,.08f),Origin.X,Origin.Y,Size.X,Size.Y);
    if (DistrictMap)
        DrawTexture(DistrictMap,Origin.X,Origin.Y,Size.X,Size.Y,(Corner.X+Radius)/(2.*Radius),
            (Corner.Y+Radius)/(2.*Radius),Span.X/(2.*Radius),Span.Y/(2.*Radius),FLinearColor::White);
    if (!GM->IsTour())
    {
        for (int32 i=1; i<GM->NavigationRoute.Num(); ++i)
        {
            const FVector& A=GM->NavigationRoute[i-1];
            const FVector& B=GM->NavigationRoute[i];
            const FVector2D From=ToMap({A.X,A.Y}), To=ToMap({B.X,B.Y});
            if (Visible(From) && Visible(To))
                DrawLine(From.X,From.Y,To.X,To.Y,FLinearColor(1.f,.65f,.12f),2.f);
        }
        FVector Target; FRotator Rotation;
        if (GM->GetNavigationTarget(Target,Rotation))
        {
            const FVector2D End=ToMap({Target.X,Target.Y});
            if (Visible(End)) DrawRect(FLinearColor::White,End.X-3,End.Y-3,6,6);
        }
    }
    const FVector2D Player=ToMap(WorldPos);
    if (Visible(Player))
    {
        const FVector Forward=V->GetActorForwardVector();
        DrawRect(FLinearColor::White,Player.X-2,Player.Y-2,4,4);
        DrawLine(Player.X,Player.Y,Player.X+Forward.X*9,Player.Y+Forward.Y*9,FLinearColor(.2f,1.f,.65f),3.f);
    }
    const double Now=GetWorld()->GetTimeSeconds();
    if (Now>=NextStreetUpdate)
    {
        if (!DistrictRoads.IsValid())
        {
            TActorIterator<ARoadNetworkActor> Road(GetWorld());
            if (Road) DistrictRoads=*Road;
        }
        CurrentStreet=DistrictRoads.IsValid() ? DistrictRoads->StreetNameAt(V->GetActorLocation()) : FString();
        NextStreetUpdate=Now+.5;
    }
    if (!CurrentStreet.IsEmpty())
        Label(CurrentStreet,Origin.X+Size.X*.5,Origin.Y+Size.Y+U*.18,.34f,FLinearColor::White);
    Label(TEXT("N"),Origin.X+Size.X-10,Origin.Y+10,.34f,FLinearColor::White);
	// ================================================================== RESULT
	if (GM->bMissionOver)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), 0.f, VS.Y * 0.33f, VS.X, VS.Y * 0.40f);
		Label(GM->EndMessage, CX, VS.Y * 0.43f, .95f, FLinearColor::White);
		Label(GM->ResultFeedback, CX, VS.Y * .50f, .5f, FLinearColor(.85f,.9f,1.f));
		Label(GM->CoachMessage, CX, VS.Y * .55f, .40f, FLinearColor(.75f,1.f,.85f));

		TArray<FDriveButton> Over;
		ADrivePlayerController::BuildOverLayout(VS, Over);
		for (const FDriveButton& B : Over)
		{
			if (B.Id == FName("NEXT_LEVEL") && !GM->CanStartNextLevel()) continue;
			Box(B.Center, B.Half, FLinearColor(0.05f, 0.35f, 0.5f, 0.85f), FLinearColor(1.f, 1.f, 1.f, 0.8f));
			Label(B.Label, B.Center.X, B.Center.Y, 0.8f, FLinearColor::White);
		}
	}
}
