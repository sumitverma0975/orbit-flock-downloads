#include "DriveHUD.h"
#include "DrivePlayerController.h"
#include "DriveVehicle.h"
#include "DriveGameMode.h"
#include "CarCatalog.h"
#include "Engine/Canvas.h"
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
		// First-run help: no car Blueprint could be loaded, so there is no pawn to drive.
		UFont* F = GEngine->GetLargeFont();
		const float Sc = Canvas->SizeY / 600.f;
		const FString Lines[] = {
			TEXT("DELHI DRIVE - no car found"),
			TEXT("Create the Blueprint /Game/Cars/BP_Car_01 (a child of DriveVehicle)."),
			TEXT("See README section 2. Also check Output Log for 'DelhiDrive'."),
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
		const FCarSpec& C = DriveCatalog::Car(GM->CarIndex);
		const bool bCarLocked = !GM->IsCarUnlocked(GM->CarIndex);
		const bool bLocked = bCarLocked || !GM->IsLevelUnlocked();
		const bool bSuper = C.Class == ECarClass::Supercar;
		const FLinearColor Accent = bSuper ? FLinearColor(1.f, 0.35f, 0.1f) : FLinearColor(1.f, 0.8f, 0.25f);
		const FLinearColor Gray(0.75f, 0.75f, 0.75f, 1.f);

		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.62f), 0.f, 0.f, VS.X, VS.Y);
		Label(TEXT("DELHI DRIVE"), CX, U * 0.6f, 1.6f, FLinearColor(1.f, 0.85f, 0.2f, 1.f));
		Label(C.Name, CX, U * 1.8f, 1.25f, bCarLocked ? FLinearColor(0.6f, 0.6f, 0.6f, 1.f) : FLinearColor::White);
		Label(bSuper ? TEXT("SUPERCAR") : TEXT("LUXURY"), CX, U * 2.35f, 0.6f, Accent);

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
		Label(FString::Printf(TEXT("%02d / %02d   %s"), GM->LevelIndex + 1, DriveLevels::NumLevels(), GM->Level().Name), CX, U * 6.35f, 0.85f, Accent);
		const int32 Stars = GM->LevelStars.IsValidIndex(GM->LevelIndex) ? GM->LevelStars[GM->LevelIndex] : 0;
		const FString LevelInfo = GM->IsLevelUnlocked()
			? FString::Printf(TEXT("%d/3 stars  |  %d checkpoints  |  crash limit %d"), Stars, GM->TotalCheckpoints, GM->Level().MaxCrashes)
			: TEXT("LEVEL LOCKED - complete the previous level");
		Label(LevelInfo, CX, U * 7.0f, 0.5f, Gray);
		Label(TEXT("OpenStreetMap roads | simulated scenery / hazards"), CX, U * 8.95f, 0.45f, Gray);
		Label(TEXT("(c) OpenStreetMap contributors | openstreetmap.org/copyright"), CX, U * 9.45f, 0.4f, Gray);

		TArray<FDriveButton> Menu;
		ADrivePlayerController::BuildMenuLayout(VS, Menu);
		static const TCHAR* const GfxNames[3] = { TEXT("GRAPHICS: LOW"), TEXT("GRAPHICS: MEDIUM"), TEXT("GRAPHICS: HIGH") };
		for (const FDriveButton& B : Menu)
		{
			FLinearColor Fill(0.f, 0.f, 0.f, 0.5f);
			FString Text = B.Label;
			if (B.Id == FName("GFX")) Text = GfxNames[FMath::Clamp(GM->Quality, 0, 2)];
			if (B.Id == FName("CITY")) Text = FString::Printf(TEXT("CITY: %s"), GM->CityName());
			if (B.Id == FName("START"))
			{
				Fill = bLocked ? FLinearColor(0.25f, 0.25f, 0.25f, 0.8f) : FLinearColor(0.05f, 0.5f, 0.12f, 0.85f);
				Text = bLocked ? TEXT("LOCKED") : TEXT("START");
			}
			Box(B.Center, B.Half, Fill, FLinearColor(1.f, 1.f, 1.f, 0.7f));
			Label(Text, B.Center.X, B.Center.Y, B.Id == FName("START") ? 1.0f : (B.Id == FName("CITY") ? 0.45f : 0.6f), FLinearColor::White);
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
	Label(GM->GetMissionStatus(), CX, U * 1.55f, 0.5f, FLinearColor::White);

	// ================================================================== RESULT
	if (GM->bMissionOver)
	{
		DrawRect(FLinearColor(0.f, 0.f, 0.f, 0.65f), 0.f, VS.Y * 0.33f, VS.X, VS.Y * 0.40f);
		Label(GM->EndMessage, CX, VS.Y * 0.45f, 1.2f, FLinearColor::White);

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
