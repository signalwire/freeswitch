<?php

namespace App\Filament\Widgets;

use App\Models\Category;
use App\Models\Post;
use App\Models\User;
use Filament\Widgets\StatsOverviewWidget;
use Filament\Widgets\StatsOverviewWidget\Stat;

class StatsOverview extends StatsOverviewWidget
{
    protected function getStats(): array
    {
        return [
            Stat::make('Total Users', User::count())
                ->description('All registered users')
                ->descriptionIcon('heroicon-m-user-group')
                ->color('success'),
            Stat::make('Total Posts', Post::count())
                ->description('All posts created')
                ->descriptionIcon('heroicon-m-document-text')
                ->color('info'),
            Stat::make('Published Posts', Post::where('is_published', true)->count())
                ->description('Posts visible to public')
                ->descriptionIcon('heroicon-m-check-circle')
                ->color('primary'),
            Stat::make('Total Categories', Category::count())
                ->description('All categories')
                ->descriptionIcon('heroicon-m-folder')
                ->color('warning'),
        ];
    }
}
