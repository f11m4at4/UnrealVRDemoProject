// Fill out your copyright notice in the Description page of Project Settings.


#include "CMenu.h"
#include "Kismet/KismetSystemLibrary.h"

void UCMenu::QuitVRGame()
{
	auto PC = GetWorld()->GetFirstPlayerController();
#if WITH_EDITOR
	UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, false);
#else
	UKismetSystemLibrary::QuitGame(GetWorld(), PC, EQuitPreference::Quit, true);
#endif
}
