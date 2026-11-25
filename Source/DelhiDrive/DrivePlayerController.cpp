#include "DrivePlayerController.h"
#include "DriveVehicle.h"
#include "DriveGameMode.h"
#include "Engine/World.h"
#include "Components/InputComponent.h"

void ADrivePlayerController::BeginPlay()
{
	Super::BeginPlay();
	ActivateTouchInterface(nullptr); // hide the default virtual joysticks
	bShowMouseCursor = false;
	SetInputMode(FInputModeGameOnly());
}

void ADrivePlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	if (InputComponent)
	{
		InputComponent->BindTouch(IE_Pressed,  this, &ADrivePlayerController::OnTouchDown);
		InputComponent->BindTouch(IE_Released, this, &ADrivePlayerController::OnTouchUp);
		InputComponent->BindTouch(IE_Repeat,   this, &ADrivePlayerController::OnTouchMove);
	}
}

// ---------------------------------------------------------------------------- layouts

void ADrivePlayerController::BuildLayout(const FVector2D& VS, bool bAuto, TArray<FDriveButton>& Out, FVector2D& WheelCenter, float& WheelRadius)
{
	const float U = VS.Y * 0.1f;
	WheelCenter = FVector2D(U * 2.9f, VS.Y - U * 2.6f);
	WheelRadius = U * 1.9f;

	Out.Reset();
	Out.Add({ FName("GAS"),   FVector2D(VS.X - U * 1.5f, VS.Y - U * 2.2f), FVector2D(U * 1.0f, U * 1.6f), TEXT("GAS"),   true });
	Out.Add({ FName("BRAKE"), FVector2D(VS.X - U * 3.9f, VS.Y - U * 1.7f), FVector2D(U * 1.3f, U * 1.1f), TEXT("BRAKE"), true });
	Out.Add({ FName("HAND"),  FVector2D(VS.X - U * 3.9f, VS.Y - U * 3.7f), FVector2D(U * 1.1f, U * 0.5f), TEXT("HAND"),  true });

	Out.Add({ FName("RECOVER"), FVector2D(VS.X - U * 6.1f, U * 2.6f), FVector2D(U * 1.3f, U * 0.5f), TEXT("RECOVER"), false });
	Out.Add({ FName("CAM"),  FVector2D(VS.X - U * 1.2f, U * 0.8f), FVector2D(U * 1.0f, U * 0.5f), TEXT("CAM"),  false });
	Out.Add({ FName("MODE"), FVector2D(VS.X - U * 3.5f, U * 0.8f), FVector2D(U * 1.0f, U * 0.5f), TEXT("MODE"), false });
	Out.Add({ FName("MENU"), FVector2D(VS.X - U * 5.8f, U * 0.8f), FVector2D(U * 1.0f, U * 0.5f), TEXT("MENU"), false });

	if (bAuto)
	{
		Out.Add({ FName("DR"), FVector2D(VS.X - U * 1.5f, U * 2.4f), FVector2D(U * 1.1f, U * 0.6f), TEXT("D/R"), false });
	}
	else
	{
		Out.Add({ FName("UP"), FVector2D(VS.X - U * 1.5f, U * 2.4f), FVector2D(U * 1.1f, U * 0.6f), TEXT("SHIFT +"), false });
		Out.Add({ FName("DN"), FVector2D(VS.X - U * 1.5f, U * 3.7f), FVector2D(U * 1.1f, U * 0.6f), TEXT("SHIFT -"), false });
	}
}

void ADrivePlayerController::BuildMenuLayout(const FVector2D& VS, TArray<FDriveButton>& Out)
{
	const float U = VS.Y * 0.1f;
	const float CX = VS.X * 0.5f;
	Out.Reset();
	Out.Add({ FName("CREDITS"), FVector2D(U * 1.55f, U * 9.4f), FVector2D(U * 1.3f, U * .45f), TEXT("CREDITS"), false });
	Out.Add({ FName("SESSION"), FVector2D(U * 2.3f, U * .65f), FVector2D(U * 2.f, U * .45f), TEXT("SESSION"), false });
	Out.Add({ FName("CITY"),      FVector2D(VS.X - U * 2.3f, U * 0.65f), FVector2D(U * 2.0f, U * 0.45f), TEXT("CITY"), false });
	Out.Add({ FName("CAR_PREV"),  FVector2D(CX - U * 5.2f, U * 1.8f),  FVector2D(U * 0.7f, U * 0.65f), TEXT("<"), false });
	Out.Add({ FName("CAR_NEXT"),  FVector2D(CX + U * 5.2f, U * 1.8f),  FVector2D(U * 0.7f, U * 0.65f), TEXT(">"), false });
	Out.Add({ FName("DIFF_PREV"), FVector2D(CX - U * 3.6f, U * 5.15f), FVector2D(U * 0.65f, U * 0.45f), TEXT("<"), false });
	Out.Add({ FName("DIFF_NEXT"), FVector2D(CX + U * 3.6f, U * 5.15f), FVector2D(U * 0.65f, U * 0.45f), TEXT(">"), false });
	Out.Add({ FName("LEVEL_PREV"),FVector2D(CX - U * 5.2f, U * 6.35f), FVector2D(U * 0.65f, U * 0.5f), TEXT("<"), false });
	Out.Add({ FName("LEVEL_NEXT"),FVector2D(CX + U * 5.2f, U * 6.35f), FVector2D(U * 0.65f, U * 0.5f), TEXT(">"), false });
	Out.Add({ FName("GFX"),       FVector2D(CX - U * 2.9f, U * 8.45f), FVector2D(U * 2.5f, U * 0.6f), TEXT("GRAPHICS"), false });
	Out.Add({ FName("START"),     FVector2D(CX + U * 2.9f, U * 8.45f), FVector2D(U * 2.5f, U * 0.6f), TEXT("START"), false });
}

void ADrivePlayerController::BuildOverLayout(const FVector2D& VS, TArray<FDriveButton>& Out)
{
	const float U = VS.Y * 0.1f;
	const float CX = VS.X * 0.5f;
	Out.Reset();
	Out.Add({ FName("RETRY"), FVector2D(CX - U * 4.2f, VS.Y * 0.63f), FVector2D(U * 1.9f, U * 0.6f), TEXT("RETRY"), false });
	Out.Add({ FName("NEXT_LEVEL"), FVector2D(CX, VS.Y * 0.63f), FVector2D(U * 1.9f, U * 0.6f), TEXT("NEXT LEVEL"), false });
	Out.Add({ FName("MENU"), FVector2D(CX + U * 4.2f, VS.Y * 0.63f), FVector2D(U * 1.9f, U * 0.6f), TEXT("MENU"), false });
}

bool ADrivePlayerController::IsHeld(FName Id) const
{
	for (const TPair<int32, FName>& KV : Held)
	{
		if (KV.Value == Id) return true;
	}
	return false;
}

float ADrivePlayerController::PedalInput(FName Id) const
{
	float Value = 0.f;
	for (const TPair<int32,FName>& Pair : Held)
	{
		if (Pair.Value == Id)
		{
			const float* Pedal = PedalValues.Find(Pair.Key);
			Value = FMath::Max(Value,Pedal ? *Pedal : 1.f);
		}
	}
	return Value;
}

void ADrivePlayerController::ResetControls()
{
	Held.Reset();
	PedalValues.Reset();
	WheelFinger = -1;
	WheelAngleDeg = 0.f;
	LastFingerAngle = 0.f;
	if (ADriveVehicle* V = Cast<ADriveVehicle>(GetPawn()))
	{
		V->SetDriveInput(0.f, 1.f, 0.f, true);
	}
}

bool ADrivePlayerController::HitAny(const TArray<FDriveButton>& Buttons, const FVector2D& P, FName& OutId) const
{
	for (const FDriveButton& B : Buttons)
	{
		if (FMath::Abs(P.X - B.Center.X) <= B.Half.X && FMath::Abs(P.Y - B.Center.Y) <= B.Half.Y)
		{
			OutId = B.Id;
			return true;
		}
	}
	return false;
}

static float FingerAngleDeg(const FVector2D& P, const FVector2D& C)
{
	return FMath::RadiansToDegrees(FMath::Atan2(P.Y - C.Y, P.X - C.X));
}

// ---------------------------------------------------------------------------- touch

void ADrivePlayerController::OnTouchDown(ETouchIndex::Type Finger, FVector Location)
{
	ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();

	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);
	const FVector2D VS(SX, SY);
	const FVector2D P(Location.X, Location.Y);
	FName Id;

	if (GM && GM->bInMenu)
	{
		if (bShowingCredits) { bShowingCredits = false; return; }
		TArray<FDriveButton> Menu;
		BuildMenuLayout(VS, Menu);
		if (HitAny(Menu, P, Id)) HandleMenuButton(Id);
		return;
	}
	if (GM && GM->bMissionOver)
	{
		TArray<FDriveButton> Over;
		BuildOverLayout(VS, Over);
		if (HitAny(Over, P, Id))
		{
			if (Id == FName("RETRY")) GM->RestartMission();
			else if (Id == FName("NEXT_LEVEL")) GM->StartNextLevel();
			else GM->OpenMenu();
		}
		return;
	}

	ADriveVehicle* V = Cast<ADriveVehicle>(GetPawn());
	if (!V) return;

	TArray<FDriveButton> Buttons;
	FVector2D WC;
	float WR = 0.f;
	BuildLayout(VS, V->IsAutomatic(), Buttons, WC, WR);

	if (WheelFinger < 0 && FVector2D::Distance(P, WC) < WR * 1.4f)
	{
		WheelFinger = static_cast<int32>(Finger);
		LastFingerAngle = FingerAngleDeg(P, WC);
		return;
	}

	for (const FDriveButton& B : Buttons)
	{
		if (FMath::Abs(P.X - B.Center.X) <= B.Half.X && FMath::Abs(P.Y - B.Center.Y) <= B.Half.Y)
		{
			if (B.bHold)
			{
				Held.Add(static_cast<int32>(Finger),B.Id);
				if (B.Id == FName("GAS") || B.Id == FName("BRAKE"))
					PedalValues.Add(static_cast<int32>(Finger),FMath::Clamp(static_cast<float>((B.Center.Y+B.Half.Y-P.Y)/(2.f*B.Half.Y)),0.f,1.f));
			}
			else HandleButton(B.Id);
			return;
		}
	}
}

void ADrivePlayerController::OnTouchUp(ETouchIndex::Type Finger, FVector Location)
{
	if (WheelFinger == static_cast<int32>(Finger)) WheelFinger = -1;
	Held.Remove(static_cast<int32>(Finger));
	PedalValues.Remove(static_cast<int32>(Finger));
}

void ADrivePlayerController::OnTouchMove(ETouchIndex::Type Finger, FVector Location)
{
	if (const FName* Pedal = Held.Find(static_cast<int32>(Finger)))
	{
		if (*Pedal == FName("GAS") || *Pedal == FName("BRAKE"))
		{
			int32 Width=0,Height=0; GetViewportSize(Width,Height);
			TArray<FDriveButton> Buttons; FVector2D Center; float Radius=0.f;
			BuildLayout(FVector2D(Width,Height),true,Buttons,Center,Radius);
			for (const FDriveButton& Button : Buttons)
				if (Button.Id == *Pedal) PedalValues.Add(static_cast<int32>(Finger),FMath::Clamp(static_cast<float>((Button.Center.Y+Button.Half.Y-Location.Y)/(2.f*Button.Half.Y)),0.f,1.f));
		}
		return;
	}
	if (WheelFinger != static_cast<int32>(Finger)) return;

	int32 SX = 0, SY = 0;
	GetViewportSize(SX, SY);
	TArray<FDriveButton> Unused;
	FVector2D WC;
	float WR = 0.f;
	BuildLayout(FVector2D(SX, SY), true, Unused, WC, WR);

	const float Angle = FingerAngleDeg(FVector2D(Location.X, Location.Y), WC);
	const float Delta = FMath::UnwindDegrees(Angle - LastFingerAngle);
	LastFingerAngle = Angle;
	WheelAngleDeg = FMath::Clamp(WheelAngleDeg + Delta, -MaxWheelDeg, MaxWheelDeg);
}

void ADrivePlayerController::HandleButton(FName Id)
{
	ADriveVehicle* V = Cast<ADriveVehicle>(GetPawn());
	if (!V) return;

	if (Id == FName("CAM"))       V->CycleCamera();
	else if (Id == FName("MODE")) V->ToggleAutomatic();
	else if (Id == FName("DR"))   V->ToggleReverse();
	else if (Id == FName("UP"))   V->ShiftUp();
	else if (Id == FName("DN"))   V->ShiftDown();
	else if (Id == FName("RECOVER"))
	{
		if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>()) GM->RecoverVehicle();
	}
	else if (Id == FName("MENU"))
	{
		if (ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>()) GM->OpenMenu();
	}
}

void ADrivePlayerController::HandleMenuButton(FName Id)
{
	ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();
	if (!GM) return;

	if (Id == FName("CREDITS")) bShowingCredits = true;
	else if (Id == FName("CAR_PREV"))        GM->ChangeCar(this, -1);   // swaps the pawn
	else if (Id == FName("CAR_NEXT"))   GM->ChangeCar(this, +1);
	else if (Id == FName("DIFF_PREV"))  GM->ChangeDifficulty(-1);
	else if (Id == FName("DIFF_NEXT"))  GM->ChangeDifficulty(+1);
	else if (Id == FName("GFX"))        GM->ChangeQuality(+1);
	else if (Id == FName("LEVEL_PREV")) GM->ChangeLevel(-1);
	else if (Id == FName("LEVEL_NEXT")) GM->ChangeLevel(+1);
	else if (Id == FName("CITY"))       GM->ChangeCity();
	else if (Id == FName("SESSION"))    GM->ChangeSession();
	else if (Id == FName("START"))      GM->StartMission();
}

// ---------------------------------------------------------------------------- per frame

void ADrivePlayerController::PlayerTick(float DT)
{
	Super::PlayerTick(DT);

	ADriveGameMode* GM = GetWorld()->GetAuthGameMode<ADriveGameMode>();

	// Keyboard: menu and result screens
	if (GM)
	{
		if (GM->bInMenu)
		{
			if (WasInputKeyJustPressed(EKeys::F2)) { bShowingCredits = !bShowingCredits; return; }
			if (bShowingCredits) { if (WasInputKeyJustPressed(EKeys::Escape) || WasInputKeyJustPressed(EKeys::Enter)) bShowingCredits = false; return; }
			if (WasInputKeyJustPressed(EKeys::Left))  GM->ChangeCar(this, -1);
			if (WasInputKeyJustPressed(EKeys::Right)) GM->ChangeCar(this, +1);
			if (WasInputKeyJustPressed(EKeys::Up))    GM->ChangeDifficulty(+1);
			if (WasInputKeyJustPressed(EKeys::Down))  GM->ChangeDifficulty(-1);
			if (WasInputKeyJustPressed(EKeys::G))     GM->ChangeQuality(+1);
			if (WasInputKeyJustPressed(EKeys::T))     GM->ChangeSession();
			if (WasInputKeyJustPressed(EKeys::Q))     GM->ChangeLevel(-1);
			if (WasInputKeyJustPressed(EKeys::E))     GM->ChangeLevel(+1);
			if (WasInputKeyJustPressed(EKeys::C))     GM->ChangeCity();
			if (WasInputKeyJustPressed(EKeys::Enter)) GM->StartMission();
		}
		else if (GM->bMissionOver)
		{
			if (WasInputKeyJustPressed(EKeys::Enter))     GM->RestartMission();
			if (WasInputKeyJustPressed(EKeys::BackSpace)) GM->OpenMenu();
			if (WasInputKeyJustPressed(EKeys::N))         GM->StartNextLevel();
		}
	}

	ADriveVehicle* V = Cast<ADriveVehicle>(GetPawn()); // fetch after any car swap above
	if (!V) return;

	if (GM && (GM->bInMenu || GM->bMissionOver))
	{
		V->SetDriveInput(0.f, 1.f, 0.f, true); // parked on menu and result screens
		return;
	}

	if (WasInputKeyJustPressed(EKeys::F1) && GM) { GM->RecoverVehicle(); return; }
	if (WasInputKeyJustPressed(EKeys::C)) V->CycleCamera();
	if (WasInputKeyJustPressed(EKeys::M)) V->ToggleAutomatic();
	if (WasInputKeyJustPressed(EKeys::R)) V->ToggleReverse();
	if (WasInputKeyJustPressed(EKeys::E)) V->ShiftUp();
	if (WasInputKeyJustPressed(EKeys::Q)) V->ShiftDown();
	if (WasInputKeyJustPressed(EKeys::Tab) && GM)
	{
		GM->OpenMenu();
		return;
	}

	float Throttle = PedalInput(FName("GAS"));
	float Brake = PedalInput(FName("BRAKE"));
	bool bHand = IsHeld(FName("HAND"));

	if (IsInputKeyDown(EKeys::W) || IsInputKeyDown(EKeys::Up))   Throttle = 1.f;
	if (IsInputKeyDown(EKeys::S) || IsInputKeyDown(EKeys::Down)) Brake = 1.f;
	if (IsInputKeyDown(EKeys::SpaceBar)) bHand = true;

	const float KeySteer =
		((IsInputKeyDown(EKeys::D) || IsInputKeyDown(EKeys::Right)) ? 1.f : 0.f) -
		((IsInputKeyDown(EKeys::A) || IsInputKeyDown(EKeys::Left)) ? 1.f : 0.f);

	if (WheelFinger < 0)
	{
		// Wheel self-centres (or follows the keyboard).
		WheelAngleDeg = FMath::FInterpTo(WheelAngleDeg, KeySteer * MaxWheelDeg, DT, KeySteer != 0.f ? 6.f : 5.f);
	}

	V->SetDriveInput(Throttle, Brake, WheelAngleDeg / MaxWheelDeg, bHand);
}
